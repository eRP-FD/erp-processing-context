#include "erp/client/request/ValidatedClientRequest.hxx"


ValidatedClientRequest::ValidatedClientRequest(const ClientRequest& aClientRequest)
: clientRequest(aClientRequest)
{
    clientRequest.getHeader().checkInvariants();
}


const Header& ValidatedClientRequest::getHeader() const
{
    return clientRequest.getHeader();
}


const std::string& ValidatedClientRequest::getBody() const
{
    return clientRequest.getBody();
}
