/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/server/response/ResponseBuilder.hxx"


class ServerRequest;
namespace model
{
class ErrorResponse;
}
class PushResponseBuilder : public ResponseBuilder
{
public:
    PushResponseBuilder(ServerResponse& innerResponse, HttpStatus httpStatus, const std::string& detailsText,
                        const std::optional<std::string>& diagnostics);
};
