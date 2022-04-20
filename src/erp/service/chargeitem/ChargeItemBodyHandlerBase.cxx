/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/service/chargeitem/ChargeItemBodyHandlerBase.hxx"
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
model::Bundle ChargeItemBodyHandlerBase::createAndValidateDispenseItem(
    const model::Binary& containedBinary,
    const PcServiceContext& serviceContext)
{
    std::string decodedBinaryData;
    try
    {
        decodedBinaryData = Base64::decodeToString(containedBinary.data().value());
    }
    catch(const std::invalid_argument& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Decoding of dispense item binary failed", exc.what());
    }
    // TODO this will only work if content type of binary is application/octet-stream
    return model::Bundle::fromXml(
            decodedBinaryData, serviceContext.getXmlValidator(),
            serviceContext.getInCodeValidator(), SchemaType::fhir/*TODO correct schema*/);
}


//static
void ChargeItemBodyHandlerBase::setDispenseItemReference(
    model::ChargeItem& chargeItem,
    const model::Bundle& dispenseItemBundle)
{
    try
    {
        chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem,
                                              "Bundle/" + dispenseItemBundle.getId().toString());
    }
    catch(const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Dispense item has no id", exc.what());
    }
}


