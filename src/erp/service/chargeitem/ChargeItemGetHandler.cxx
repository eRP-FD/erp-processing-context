/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/TLog.hxx"


namespace
{
    // GEMREQ-start A_22127#signBundle
    template <typename BundleT>
    void signBundle(BundleT& bundle, const std::string& authorIdentifier, const PcServiceContext& serviceContext)
    {
        CadesBesSignature cadesBesSignature{serviceContext.getCFdSigErp(),
                                            serviceContext.getCFdSigErpPrv(),
                                            bundle.serializeToXmlString(),
                                            std::nullopt,
                                            serviceContext.getCFdSigErpManager().getOcspResponse()};

        const model::Signature fullSignature{cadesBesSignature.getBase64(),
                                             model::Timestamp::now(),
                                             authorIdentifier};

        bundle.setSignature(fullSignature);
    }
    // GEMREQ-end A_22127#signBundle

    // GEMREQ-start A_22127#counterSignDispenseItem
    void counterSignDispenseItem(const model::Binary& dispenseItem,
                                 model::AbgabedatenPkvBundle& dispenseItemBundle,
                                 const std::string& authorIdentifier,
                                 const PcServiceContext& serviceContext)
    {
        CadesBesSignature dispenseItemCadesBesSignature{dispenseItem.data().value().data()};
        dispenseItemCadesBesSignature.addCounterSignature(serviceContext.getCFdSigErp(),
                                                          serviceContext.getCFdSigErpPrv());

        Expect3(dispenseItem.id().has_value(), "Dispense item has no ID", std::logic_error);

        const model::Signature signature{dispenseItemCadesBesSignature.getBase64(),
                                         model::Timestamp::now(),
                                         authorIdentifier};

        dispenseItemBundle.setSignature(signature);
    }
    // GEMREQ-end A_22127#counterSignDispenseItem
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

    // GEMREQ-start A_22119#read-kvnr
    const std::optional<std::string> idNumberClaim =
        session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(idNumberClaim.has_value(), "JWT does not contain idNumberClaim");
    // GEMREQ-end A_22119#read-kvnr

    const model::Kvnr kvnr{*idNumberClaim};

    A_22121.start("Search parameters for ChargeItem");
    auto arguments = std::optional<UrlArguments>(
        ::std::in_place,
        ::std::vector<::SearchParameter>{{"entered-date", "entered_date", ::SearchParameter::Type::Date},
                                         {"_lastUpdated", "last_modified", ::SearchParameter::Type::Date}});
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());

    // GEMREQ-start A_22119#call-database
    A_22119.start("Assure identical kvnr of access token and ChargeItem resource");
    chargeItems = session.database()->retrieveAllChargeItemsForInsurant(kvnr, arguments);
    A_22119.finish();
    // GEMREQ-end A_22119#call-database
    A_22121.finish();
    totalSearchMatches = responseIsPartOfMultiplePages(arguments->pagingArgument(), chargeItems.size())
                             ? session.database()->countChargeInformationForInsurant(kvnr, arguments)
                             : chargeItems.size();
    links = arguments->getBundleLinks(getLinkBase(), "/ChargeItem", totalSearchMatches);

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
        A_22122.start("Do not add referenced supporting resources.");
        std::optional<model::PrescriptionId> id = chargeItem.id();
        ModelExpect(id.has_value(), "No Id assigned to chargeItem");
        bundle.addResource(
            linkBase + "/" + id.value().toString(),
            {},
            model::Bundle::SearchMode::match,
            chargeItem.jsonDocument());
        A_22122.finish();
    }
    return bundle;
}

//--------------------------------------------------------------------------------------------------------------------

ChargeItemGetByIdHandler::ChargeItemGetByIdHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemHandlerBase(Operation::GET_ChargeItem_id, allowedProfessionOiDs)
{
}

// GEMREQ-start A_22125
// GEMREQ-start A_22126#start
void ChargeItemGetByIdHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);

    const auto idNumberClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(idNumberClaim.has_value(), "JWT does not contain idNumberClaim");

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect(professionOIDClaim.has_value(), "JWT does not contain professionOIDClaim");

    model::AuditEventId auditEventId{};

    // GEMREQ-start A_22127#buildResponse
    // response bundle
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
    auto chargeInformation = session.database()->retrieveChargeInformation(prescriptionId);

    Expect3(chargeInformation.prescription.has_value(), "Prescription has no data", ::std::logic_error);
    Expect3(chargeInformation.prescription->id().has_value(), "Prescription binary has no id", ::std::logic_error);
    Expect3(chargeInformation.prescription->data().has_value(), "Prescription binary has no data",
            ::std::logic_error);
    Expect3(chargeInformation.unsignedDispenseItem.has_value(), "Dispense Item not present", ::std::logic_error);
    const CadesBesSignature cadesBesSignature(std::string(chargeInformation.prescription.value().data().value()),
                                              session.serviceContext.getTslManager());
    auto kbvBundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());
    const auto authorIdentifier = model::Device::createReferenceString(getLinkBase());

    if (professionOIDClaim == profession_oid::oid_versicherter)
    {
// GEMREQ-end A_22126#start
        A_22125.start("Assure identical kvnr of access token and ChargeItem resource");
        Expect(chargeInformation.chargeItem.subjectKvnr().has_value(), "Retrieved ChargeItem has no kvnr");
        ErpExpect(chargeInformation.chargeItem.subjectKvnr().value() == idNumberClaim.value(), HttpStatus::Forbidden,
                  "Mismatch between access token and Kvnr");
        A_22125.finish();
// GEMREQ-end A_22125

        A_22127.start("Response for insured");
        // GEMREQ-end A_22127#buildResponse

        // GEMREQ-start A_22127#prescription
        signBundle(kbvBundle, authorIdentifier, session.serviceContext);
        // GEMREQ-end A_22127#prescription

        // GEMREQ-start A_22127#receipt
        Expect3(chargeInformation.receipt.has_value(), "Receipt not present", ::std::logic_error);
        chargeInformation.receipt->removeSignature();
        signBundle(*chargeInformation.receipt, authorIdentifier, session.serviceContext);
        // GEMREQ-end A_22127#receipt

        // GEMREQ-start A_22127#dispenseItem
        chargeInformation.unsignedDispenseItem->removeSignature();
        counterSignDispenseItem(chargeInformation.dispenseItem.value(),
                                chargeInformation.unsignedDispenseItem.value(),
                                authorIdentifier,
                                session.serviceContext);
        // GEMREQ-end A_22127#dispenseItem

        responseBundle.addResource(getLinkBase() + "/ChargeItem/" + prescriptionId.toString(), {}, {},
                                   chargeInformation.chargeItem.jsonDocument());

        const auto dispenseItemBundleSupportingReference = chargeInformation.chargeItem.supportingInfoReference(
            model::ChargeItem::SupportingInfoType::dispenseItemBundle);
        ErpExpect(dispenseItemBundleSupportingReference.has_value(), HttpStatus::InternalServerError,
                  "no supporting reference for dispense bundle present in charge item");
        responseBundle.addResource(std::string{dispenseItemBundleSupportingReference.value()}, {}, {},
                                   chargeInformation.unsignedDispenseItem->jsonDocument());

        const auto kbvBundleSupportingReference = chargeInformation.chargeItem.supportingInfoReference(
            model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
        ErpExpect(kbvBundleSupportingReference.has_value(), HttpStatus::InternalServerError,
                  "no supporting reference for prescription bundle present in charge item");
        responseBundle.addResource(std::string{kbvBundleSupportingReference.value()}, {}, {},
                                   kbvBundle.jsonDocument());

        const auto receiptSupportingReference = chargeInformation.chargeItem.supportingInfoReference(
            model::ChargeItem::SupportingInfoType::receiptBundle);
        ErpExpect(receiptSupportingReference.has_value(), HttpStatus::InternalServerError,
                  "no supporting reference for receipt bundle present in charge item");
        responseBundle.addResource(std::string{receiptSupportingReference.value()}, {}, {},
                                   chargeInformation.receipt->jsonDocument());

        A_22127.finish();

        auditEventId = model::AuditEventId::GET_ChargeItem_id_insurant;
// GEMREQ-start A_22126#id-check
    }

    else
    {
        A_22126.start("Assure identical TelematikId of access token and ChargeItem resource");
        ErpExpect(chargeInformation.chargeItem.entererTelematikId() == idNumberClaim.value(), HttpStatus::Forbidden,
                  "Mismatch between access token and TelematikId");
        A_22126.finish();
// GEMREQ-end A_22126#id-check

        A_22611_02.start("verify pharmacy access code");
        ChargeItemHandlerBase::verifyPharmacyAccessCode(session.request, chargeInformation.chargeItem);
        A_22611_02.finish();

        A_22128_01.start("Response for pharmacy. Omit receipt and access code.");
        chargeInformation.chargeItem.deleteAccessCode();
        responseBundle.addResource(getLinkBase() + "/ChargeItem/" + prescriptionId.toString(), {}, {},
                                   chargeInformation.chargeItem.jsonDocument());

        // embed QES bundle into signature of KBV-Bundle which is intentionally data duplication.
        const model::Signature prescriptionSignature{
            chargeInformation.prescription.value().data().value(),
            cadesBesSignature.getSigningTime().value_or(model::Timestamp::now()), std::nullopt, "Arzt"};
        kbvBundle.setSignature(prescriptionSignature);
        const auto kbvBundleSupportingReference = chargeInformation.chargeItem.supportingInfoReference(
            model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
        ErpExpect(kbvBundleSupportingReference.has_value(), HttpStatus::InternalServerError,
                  "no supporting reference for prescription bundle present in charge item");
        responseBundle.addResource(std::string{kbvBundleSupportingReference.value()}, {}, {},
                                   kbvBundle.jsonDocument());

        // embed signed dispense into signature of KBV-Bundle which is intentionally data duplication.
        Expect3(chargeInformation.dispenseItem.has_value(), "Dispense Item not present", ::std::logic_error);
        const CadesBesSignature dispenseItemCadesBesSignature{chargeInformation.dispenseItem->data().value().data(),
                                                              session.serviceContext.getTslManager(), true};
        const model::Signature dispenseItemSignature{
            dispenseItemCadesBesSignature.getBase64(),
            dispenseItemCadesBesSignature.getSigningTime().value_or(model::Timestamp::now()), std::nullopt, "Apotheke"};
        chargeInformation.unsignedDispenseItem->setSignature(dispenseItemSignature);

        auto dispenseItemBundleSupportingReference = chargeInformation.chargeItem.supportingInfoReference(
            model::ChargeItem::SupportingInfoType::dispenseItemBundle);
        ErpExpect(dispenseItemBundleSupportingReference.has_value(), HttpStatus::InternalServerError,
                  "no supporting reference for dispense bundle present in charge item");
        responseBundle.addResource(std::string{*dispenseItemBundleSupportingReference}, {}, {},
                                   chargeInformation.unsignedDispenseItem->jsonDocument());
        A_22128_01.finish();

        auditEventId = model::AuditEventId::GET_ChargeItem_id_pharmacy;
    }

    makeResponse(session, HttpStatus::OK, &responseBundle);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(auditEventId)
        .setInsurantKvnr(chargeInformation.chargeItem.subjectKvnr().value())
        .setAction(model::AuditEvent::Action::read);
}
