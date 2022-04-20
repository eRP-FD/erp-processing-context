/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"


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

