/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OuterResponseErrorData.hxx"
#include "erp/util/RapidjsonDocument.hxx"

namespace model
{

namespace
{

// definition of JSON pointers:
const rapidjson::Pointer xRequestIdPointer ("/x-request-id");
const rapidjson::Pointer statusPointer ("/status");
const rapidjson::Pointer errorPointer ("/error");
const rapidjson::Pointer messagePointer ("/message");

}  // anonymous namespace


OuterResponseErrorData::OuterResponseErrorData(const std::string_view& xRequestId, HttpStatus status,
                                               const std::string_view& error,
                                               const std::optional<std::string_view>& message)
    : ResourceBase(ResourceBase::NoProfile)
{
    setXRequestId(xRequestId);
    setStatus(status);
    setError(error);
    if(message.has_value())
        setMessage(message.value());
}

std::string_view OuterResponseErrorData::xRequestId() const
{
    return getStringValue(xRequestIdPointer);
}

HttpStatus OuterResponseErrorData::status() const
{
    return static_cast<HttpStatus>(getIntValue(statusPointer));
}

std::string_view OuterResponseErrorData::error() const
{
    return getStringValue(errorPointer);
}

std::optional<std::string_view> OuterResponseErrorData::message() const
{
    return getOptionalStringValue(messagePointer);
}

void OuterResponseErrorData::setXRequestId(const std::string_view& xRequestId)
{
    setValue(xRequestIdPointer, xRequestId);
}

void OuterResponseErrorData::setStatus(HttpStatus status)
{
    setValue(statusPointer, static_cast<int>(status));
}

void OuterResponseErrorData::setError(const std::string_view& error)
{
    setValue(errorPointer, error);
}

void OuterResponseErrorData::setMessage(const std::string_view& message)
{
    setValue(messagePointer, message);
}

}  // namespace model
