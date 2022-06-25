/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/TLog.hxx"


namespace
{
    template <typename BundleT>
    void signBundle(BundleT& bundle, const std::string& authorIdentifier, const PcServiceContext& serviceContext)
    {
        CadesBesSignature cadesBesSignature{serviceContext.getCFdSigErp(),
                                            serviceContext.getCFdSigErpPrv(),
                                            bundle.serializeToXmlString(),
                                            std::nullopt,
                                            serviceContext.getCFdSigErpManager().getOcspResponse()};

        const model::Signature fullSignature{Base64::encode(cadesBesSignature.get()),
                                             model::Timestamp::now(),
                                             authorIdentifier};

        bundle.setSignature(fullSignature);
    }
}


ChargeItemGetAllHandler::ChargeItemGetAllHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemHandlerBase(Operation::GET_ChargeItem, allowedProfessionOiDs)
{
}

void ChargeItemGetAllHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    std::vector<model::ChargeItem> chargeItems;
    std::size_t totalSearchMatches{0};
    std::unordered_map<model::Link::Type, std::string> links;

    const std::optional<std::string> idNumberClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(idNumberClaim.has_value(), "JWT does not contain idNumberClaim");

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect(professionOIDClaim.has_value(), "JWT does not contain professionOIDClaim");

    if(professionOIDClaim == profession_oid::oid_versicherter)
    {
        A_22121.start("Search parameters for ChargeItem");
        auto arguments = std::optional<UrlArguments>(
            std::in_place,
            std::vector<SearchParameter>
            {
                {"entered-date", "charge_item_entered_date", SearchParameter::Type::Date}
            });
        arguments->parse(session.request, session.serviceContext.getKeyDerivation());

        A_22119.start("Assure identical kvnr of access token and ChargeItem resource");
        chargeItems = session.database()->retrieveAllChargeItemsForInsurant(idNumberClaim.value(), arguments);
        A_22119.finish();
        A_22121.finish();
        totalSearchMatches =
            responseIsPartOfMultiplePages(arguments->pagingArgument(),
                                          chargeItems.size()) ?
                session.database()->countChargeInformationForInsurant(idNumberClaim.value(), arguments) :
                chargeItems.size();
        links = arguments->getBundleLinks(getLinkBase(), "/ChargeItem", totalSearchMatches);
    }
    else
    {
        A_22121.start("Search parameters for ChargeItem");
        auto arguments = std::optional<UrlArguments>(
            std::in_place,
            std::vector<SearchParameter>
            {
                { "entered-date", "charge_item_entered_date", SearchParameter::Type::Date },
                { "patient", "kvnr_hashed", SearchParameter::Type::HashedIdentity }
            });
        arguments->parse(session.request, session.serviceContext.getKeyDerivation());

        A_22120.start("Assure identical TelematikId of access token and ChargeItem resource");
        chargeItems = session.database()->retrieveAllChargeItemsForPharmacy(idNumberClaim.value(), arguments);
        A_22120.finish();
        A_22121.finish();

        totalSearchMatches =
             responseIsPartOfMultiplePages(arguments->pagingArgument(),
                                           chargeItems.size()) ?
                 session.database()->countChargeInformationForPharmacy(idNumberClaim.value(), arguments) :
                 chargeItems.size();

        links = arguments->getBundleLinks(getLinkBase(), "/ChargeItem", totalSearchMatches);
    }

    A_22123.start("Create bundle for paging");
    auto bundle = createBundle(chargeItems);
    bundle.setTotalSearchMatches(totalSearchMatches);

    for (const auto& link : links)
    {
        bundle.setLink(link.first, link.second);
    }

    makeResponse(session, HttpStatus::OK, &bundle);
    A_22123.finish();
}

model::Bundle ChargeItemGetAllHandler::createBundle(std::vector<model::ChargeItem>& chargeItems)
{
    const std::string linkBase = getLinkBase() + "/ChargeItem";
    model::Bundle bundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
    for (auto& chargeItem : chargeItems)
    {
        A_22122.start("Delete all supportingInfoType");
        chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::prescriptionItem);
        chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::receipt);
        chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::dispenseItem);
        A_22122.finish();

        std::optional<model::PrescriptionId> id = chargeItem.id();
        ModelExpect(id.has_value(), "No Id assigned to chargeItem");
        bundle.addResource(
            linkBase + "/" + id.value().toString(),
            {},
            model::Bundle::SearchMode::match,
            chargeItem.jsonDocument());
    }
    return bundle;
}

//--------------------------------------------------------------------------------------------------------------------

ChargeItemGetByIdHandler::ChargeItemGetByIdHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemHandlerBase(Operation::GET_ChargeItem_id, allowedProfessionOiDs)
{
}

void ChargeItemGetByIdHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);

    const auto idNumberClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(idNumberClaim.has_value(), "JWT does not contain idNumberClaim");

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect(professionOIDClaim.has_value(), "JWT does not contain professionOIDClaim");

    // response bundle
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
    if (professionOIDClaim == profession_oid::oid_versicherter)
    {
        auto [prescription, receipt, chargeItem, dispenseItem] = session.database()->retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt(prescriptionId);
        ErpExpect(prescription.has_value(), HttpStatus::NotFound, "No prescription found");
        ErpExpect(receipt.has_value(), HttpStatus::NotFound, "No receipt found");
        ErpExpect(chargeItem.has_value(), HttpStatus::NotFound, "No chargeItem found");
        ErpExpect(dispenseItem.has_value(), HttpStatus::NotFound, "No dispenseItem found");

        A_22125.start("Assure identical kvnr of access token and ChargeItem resource");
        ErpExpect(chargeItem.value().subjectKvnr() == idNumberClaim.value(), HttpStatus::Forbidden,
                    "Mismatch between access token and Kvnr");
        A_22125.finish();

        A_22127.start("Response for insured");
        Expect3(prescription.value().data().has_value(), "Prescription binary has no data", std::logic_error);

        const CadesBesSignature cadesBesSignature(
            std::string(prescription.value().data().value()), session.serviceContext.getTslManager());
        auto kbvBundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());

        const auto authorIdentifier = model::Device::createReferenceString(getLinkBase());
        signBundle(kbvBundle, authorIdentifier, session.serviceContext);

        receipt->removeSignature();
        signBundle(*receipt, authorIdentifier, session.serviceContext);

        responseBundle.addResource(getLinkBase() + "/ChargeItem/" + prescriptionId.toString(),
                                   {}, {}, chargeItem.value().jsonDocument());
        responseBundle.addResource(dispenseItem.value().getId().toUrn(), {}, {}, dispenseItem.value().jsonDocument());
        responseBundle.addResource(kbvBundle.getId().toUrn(), {}, {}, kbvBundle.jsonDocument());
        responseBundle.addResource(receipt.value().getId().toUrn(), {}, {}, receipt.value().jsonDocument());

        A_22127.finish();
    }
    else
    {
        auto [chargeItem, dispenseItem] = session.database()->retrieveChargeInformation(prescriptionId);
        A_22126.start("Assure identical TelematikId of access token and ChargeItem resource");
        ErpExpect(chargeItem.entererTelematikId() == idNumberClaim.value(), HttpStatus::Forbidden,
                "Mismatch between access token and TelematikId");
        A_22126.finish();

        A_22611.start("verify pharmacy access code");
        ChargeItemHandlerBase::verifyPharmacyAccessCode(session.request, chargeItem);
        A_22611.finish();

        A_22128.start("Response for pharmacy. Delete unused supportingInfoType");
        chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::prescriptionItem);
        chargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::receipt);

        responseBundle.addResource(getLinkBase() + "/ChargeItem/" + prescriptionId.toString(),
                                   {}, {}, chargeItem.jsonDocument());
        responseBundle.addResource(dispenseItem.getId().toUrn(), {}, {}, dispenseItem.jsonDocument());
        A_22128.finish();
    }

    makeResponse(session, HttpStatus::OK, &responseBundle);
}
