#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_VALIDATEDSERVERRESPONSE_HXX

#include "erp/server/response/ServerResponse.hxx"


// Server response wrapper which validates the header invariants implicitly in constructor:

class ValidatedServerResponse
{
public:
    explicit ValidatedServerResponse(const ServerResponse& aServerResponse);

    const Header& getHeader() const;

    const std::string& getBody() const;

private:
    const ServerResponse& serverResponse;
};


#endif
