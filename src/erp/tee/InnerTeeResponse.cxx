/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tee/InnerTeeResponse.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/util/ByteHelper.hxx"


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
