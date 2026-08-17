/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/network/message/HttpStatus.hxx"

#include <string>


class ErpException;
namespace model
{
// https://github.com/gematik/gem-push-notifications-concept/blob/1.2/docs_sources/definitions/error.yaml
class ErrorResponse
{
public:
    enum class ErrorCode
    {
        missingParameter,
        invalidParameter,
        malformedRequest,
        invalidAccessToken,
        invalidOid,
        notFound,
        methodNotAllowed,
        notAcceptable,
        timeout,
        rateLimitExceeded,
        internalServerError
    };
    explicit ErrorResponse(ErrorCode errorCode, std::string errorDetail);

    static ErrorResponse fromHttpStatus(HttpStatus httpStatus, const std::string& detail);

    std::string serializeToJsonString() const;

private:
    ErrorCode mErrorCode;
    std::string mErrorDetail;
};
}
