/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tee/InnerTeeResponse.hxx"

#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/server/response/ValidatedServerResponse.hxx"
#include "shared/server/response/ServerResponseWriter.hxx"
#include "shared/util/ByteHelper.hxx"


InnerTeeResponse::InnerTeeResponse (const std::string& requestId, const Header& header, const std::string& body)
    : mA("1 "
         + ByteHelper::toHex(requestId) + " "
         + ServerResponseWriter().toString(ValidatedServerResponse(ServerResponse(header, body))))
{
}


const std::string& InnerTeeResponse::getA (void) const
{
    return mA;
}
