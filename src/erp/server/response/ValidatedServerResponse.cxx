#include "erp/server/response/ValidatedServerResponse.hxx"


ValidatedServerResponse::ValidatedServerResponse(const ServerResponse& aServerResponse)
: serverResponse(aServerResponse)
{
    serverResponse.getHeader().checkInvariants();
}


const Header& ValidatedServerResponse::getHeader() const
{
    return serverResponse.getHeader();
}


const std::string& ValidatedServerResponse::getBody() const
{
    return serverResponse.getBody();
}
