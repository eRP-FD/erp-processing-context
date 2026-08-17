/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/model/push/ErrorResponse.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/util/ErpException.hxx"

#include <magic_enum/magic_enum.hpp>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <map>
#include <utility>

namespace model
{

ErrorResponse::ErrorResponse(ErrorCode errorCode, std::string errorDetail)
    : mErrorCode(errorCode)
    , mErrorDetail(std::move(errorDetail))
{
}

ErrorResponse ErrorResponse::fromHttpStatus(HttpStatus httpStatus, const std::string& detail)
{
    static const std::map<HttpStatus, ErrorCode> mapping{
        {HttpStatus::BadRequest, ErrorCode::malformedRequest},
        {HttpStatus::Unauthorized, ErrorCode::invalidAccessToken},
        {HttpStatus::Forbidden, ErrorCode::invalidOid},
        {HttpStatus::MethodNotAllowed, ErrorCode::methodNotAllowed},
        {HttpStatus::NotAcceptable, ErrorCode::notAcceptable},
        {HttpStatus::RequestTimeout, ErrorCode::timeout},
        {HttpStatus::TooManyRequests, ErrorCode::rateLimitExceeded},
        {HttpStatus::InternalServerError, ErrorCode::internalServerError}};
    const auto it = mapping.find(httpStatus);
    if (it != mapping.end())
    {
        return ErrorResponse(it->second, detail);
    }
    return ErrorResponse(ErrorCode::malformedRequest, detail);
}

std::string ErrorResponse::serializeToJsonString() const
{
    rapidjson::Document doc;
    doc.SetObject();
    const auto errorCode = magic_enum::enum_name(mErrorCode);
    doc.AddMember("errorCode", rapidjson::StringRef(errorCode.data(), errorCode.size()), doc.GetAllocator());
    if (! mErrorDetail.empty())
    {
        doc.AddMember("errorDetail", rapidjson::StringRef(mErrorDetail.c_str()), doc.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

}
