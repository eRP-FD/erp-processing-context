/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/push/PushResponseBuilder.hxx"
#include "erp/model/push/ErrorResponse.hxx"

#include <fmt/format.h>

PushResponseBuilder::PushResponseBuilder(ServerResponse& innerResponse, HttpStatus httpStatus,
                                         const std::string& detailsText, const std::optional<std::string>& diagnostics)
    : ResponseBuilder(innerResponse)
{
    status(httpStatus).clearBody().keepAlive(false);
    const auto errorResponse =
        model::ErrorResponse::fromHttpStatus(httpStatus, fmt::format("{} {}", detailsText, diagnostics.value_or("")));
    jsonBody(errorResponse.serializeToJsonString());
}
