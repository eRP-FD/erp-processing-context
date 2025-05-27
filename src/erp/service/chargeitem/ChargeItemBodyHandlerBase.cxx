/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ChargeItem.hxx"
#include "erp/service/chargeitem/ChargeItemBodyHandlerBase.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/model/Binary.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"


ChargeItemBodyHandlerBase::ChargeItemBodyHandlerBase(
    Operation operation,
    const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ChargeItemHandlerBase(operation, allowedProfessionOIDs)
{
}


// GEMREQ-start A_22137#get-dispense-item
std::optional<model::Binary> ChargeItemBodyHandlerBase::getDispenseItemBinary(const model::ChargeItem& chargeItem)
{
    std::optional<model::Binary> containedBinary;
    try
    {
        containedBinary = chargeItem.containedBinary();
        const auto reference =
            chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary);
        ErpExpect(reference.has_value(), HttpStatus::BadRequest, "No Reference to contained Binary");
        ErpExpect(containedBinary.has_value(), HttpStatus::BadRequest, "Contained binary could not be resolved or is missing");
        ErpExpect(reference.value() == std::string{"#"} + std::string{containedBinary->id().value()},
                  HttpStatus::BadRequest, "Reference to contained binary does not match contained.Binary.id");
        ErpExpect(containedBinary->data().has_value(), HttpStatus::BadRequest, "Contained binary has no data");
        return containedBinary;
    }
    catch(const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Processing of contained dispense item binary failed", exc.what());
    }
}
// GEMREQ-end A_22137#get-dispense-item

// GEMREQ-start A_22141#chargeItemCadesBes, A_22140, A_22150, A_22151, A_20159-03#validatedBundleFromSigned
/**
 * Returns the validated payload from the signed binary as a model::Bundle,
 * as well as the signed binary itself (in base64 format) after possibly having
 * been altered by adding an OCSP response to it in case it did not already contain one.
 */
std::pair<model::AbgabedatenPkvBundle, std::string> ChargeItemBodyHandlerBase::validatedBundleFromSigned(
    const ::model::Binary& containedBinary,
    SessionContext& sessionContext,
    ::VauErrorCode onError)
{
    try
    {
        const CadesBesSignature cadesBesSignature{std::string{containedBinary.data().value()},
                                                  sessionContext.serviceContext.getTslManager(), true};
        // parse it without validation, at this point a fhir document is fine
        auto pkvBundleFactory = model::ResourceFactory<model::AbgabedatenPkvBundle>::fromXml(cadesBesSignature.payload(),
                                                                                      sessionContext.serviceContext.getXmlValidator());
        if (const auto profileVersion = pkvBundleFactory.profileVersion())
        {
            sessionContext.addOuterResponseHeaderField(Header::DavAbgabedaten, to_string(*profileVersion));
        }
        const auto repoView = pkvBundleFactory.getValidationView();
        return std::make_pair(std::move(pkvBundleFactory).getValidated(model::ProfileType::DAV_PKV_PR_ERP_AbgabedatenBundle, repoView),
                              cadesBesSignature.getBase64());
    }
    catch (const TslError& ex)
    {
        std::string description;
        // Use the first error data as the description, if available. Otherwise fall back
        // to the full exception description
        if (! ex.getErrorData().empty())
        {
            description = ex.getErrorData()[0].message;
        }
        else
        {
            description = ex.what();
        }
        VauFail(ex.getHttpStatus(), onError, description);
    }
    catch (const CadesBesSignature::UnexpectedProfessionOidException& ex)
    {
        VauFail(HttpStatus::BadRequest, onError, ex.what());
    }
    catch (const CadesBesSignature::VerificationException& ex)
    {
        VauFail(HttpStatus::BadRequest, onError, ex.what());
    }
    catch (const ErpException& ex)
    {
        TVLOG(1) << "ErpException: " << ex.what();
        VauFailWithDiagnostics(ex.status(), onError, ex.what(), ex.diagnostics());
    }
    catch(const model::ModelException& mex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing / validation error", mex.what());
    }
    catch(const std::invalid_argument& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Decoding of dispense item binary failed", exc.what());
    }
    catch (const std::exception& ex)
    {
        VauFail(HttpStatus::InternalServerError, onError, ex.what());
    }
    catch (...)
    {
        VauFail(HttpStatus::InternalServerError, onError, "unexpected throwable");
    }
}
// GEMREQ-end A_22141#chargeItemCadesBes, A_22140, A_22150, A_22151, A_20159-03#validatedBundleFromSigned
