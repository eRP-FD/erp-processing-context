/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/util/ByteHelper.hxx"


namespace
{

model::PrescriptionId parseId(
    const std::optional<std::string>& prescriptionIdValue,
    AccessLog& accessLog,
    const std::string& paramName)
{
    ErpExpect(prescriptionIdValue.has_value(), HttpStatus::BadRequest, paramName + " URL parameter is missing");
    try
    {
        auto prescriptionId = model::PrescriptionId::fromString(prescriptionIdValue.value());
        accessLog.prescriptionId(prescriptionIdValue.value());
        return prescriptionId;
    }
    catch (const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Failed to parse prescription ID from URL parameter", exc.what());
    }
}

} // anonymous namespace


ChargeItemHandlerBase::ChargeItemHandlerBase(
    Operation operation,
    const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(operation, allowedProfessionOIDs)
{
}


// static
model::PrescriptionId ChargeItemHandlerBase::parseIdFromPath(
    const ServerRequest& request,
    AccessLog& accessLog,
    const std::string& paramName)
{
    return parseId(request.getPathParameter(paramName), accessLog, paramName);
}


// static
model::PrescriptionId ChargeItemHandlerBase::parseIdFromQuery(
    const ServerRequest& request,
    AccessLog& accessLog,
    const std::string& paramName)
{
    return parseId(request.getQueryParameter(paramName), accessLog, paramName);
}

// static
void ChargeItemHandlerBase::verifyPharmacyAccessCode(
    const ServerRequest& request,
    const model::ChargeItem& chargeItem,
    const bool tryHttpHeader)
{
    auto pharmacyAccessCode = request.getQueryParameter("ac");
    if (!pharmacyAccessCode.has_value() && tryHttpHeader)
    {
        pharmacyAccessCode = request.header().header(Header::XAccessCode);
    }

    ErpExpect(pharmacyAccessCode.has_value(),
              HttpStatus::Forbidden,
              "No access code provided for charge item");

    const auto chargeItemAccessCode = chargeItem.accessCode();
    ErpExpect(chargeItemAccessCode.has_value(),
              HttpStatus::Forbidden,
              "Charge item does not have an associated access code");

    ErpExpect(*pharmacyAccessCode == *chargeItemAccessCode,
              HttpStatus::Forbidden,
              "Provided charge item access code is wrong");
}


// static
// GEMREQ-start createPharmacyAccessCode
std::string ChargeItemHandlerBase::createPharmacyAccessCode()
{
    const auto rawAccessCode = SecureRandomGenerator::generate(32);
    return ByteHelper::toHex(rawAccessCode);
}
// GEMREQ-end createPharmacyAccessCode
