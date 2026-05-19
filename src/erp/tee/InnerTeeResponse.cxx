/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tee/InnerTeeResponse.hxx"

#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/server/response/ValidatedServerResponse.hxx"
#include "shared/server/response/ServerResponseWriter.hxx"
#include "shared/util/ByteHelper.hxx"


// GEMREQ-start A_20163
InnerTeeResponse::InnerTeeResponse (const std::string& requestId, const Header& header, const std::string& body)
    : mA("1 "
         + ByteHelper::toHex(requestId) + " "
         + ServerResponseWriter().toString(ValidatedServerResponse(ServerResponse(header, body))))
{
}
// GEMREQ-end A_20163


const std::string& InnerTeeResponse::getA (void) const
{
    return mA;
}
