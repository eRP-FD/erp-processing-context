/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/service/chargeitem/ChargeItemBodyHandlerBase.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Base64.hxx"


ChargeItemBodyHandlerBase::ChargeItemBodyHandlerBase(
    Operation operation,
    const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ChargeItemHandlerBase(operation, allowedProfessionOIDs)
{
}


// static
std::optional<model::Binary> ChargeItemBodyHandlerBase::getDispenseItemBinary(const model::ChargeItem& chargeItem)
{
    std::optional<model::Binary> containedBinary;
    try
    {
        containedBinary = chargeItem.containedBinary();
        ErpExpect(containedBinary.has_value(), HttpStatus::BadRequest, "No contained binary provided");
        ErpExpect(containedBinary->data().has_value(), HttpStatus::BadRequest, "Contained binary has no data");
        return containedBinary;
    }
    catch(const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Processing of contained dispense item binary failed", exc.what());
    }
}


// static
::model::Bundle ChargeItemBodyHandlerBase::validatedBundleFromSigned(const ::model::Binary& containedBinary,
                                                                     ::SchemaType schemaType,
                                                                     ::PcServiceContext& serviceContext,
                                                                     ::VauErrorCode onError)
{
    try
    {
        const CadesBesSignature cadesBesSignature{containedBinary.data().value().data(),
                                                  serviceContext.getTslManager(),
                                                  true};
        return model::Bundle::fromXml(cadesBesSignature.payload(), serviceContext.getXmlValidator(),
                                      serviceContext.getInCodeValidator(), schemaType, false);
    }
    catch (const TslError& ex)
    {
        VauFail(ex.getHttpStatus(), onError, ex.what());
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
        VauFail(ex.status(), onError, "ErpException");
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
