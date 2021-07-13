#include "erp/tee/InnerTeeResponse.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/server/response/ServerResponseWriter.hxx"
#include "erp/util/ByteHelper.hxx"


InnerTeeResponse::InnerTeeResponse (const std::string& requestId, const Header& header, const std::string& body)
    : mA("1 "
         + ByteHelper::toHex(requestId) + " "
         + ServerResponseWriter(ValidatedServerResponse(ServerResponse(header, body))).toString())
{
}


const std::string& InnerTeeResponse::getA (void) const
{
    return mA;
}
