/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "shared/server/BaseSessionContext.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "request/ServerRequest.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/util/Expect.hxx"


model::Kvnr BaseSessionContext::kvnrFromAccessToken() const
{
    const auto& accessToken = request.getAccessToken();
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpectWithDiagnostics(kvnrClaim.has_value(), HttpStatus::BadRequest,
                             std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::missingParameter)},
                             "Missing claim in ACCESS_TOKEN: " + std::string(JWT::idNumberClaim));
    return model::Kvnr{*kvnrClaim};
}
