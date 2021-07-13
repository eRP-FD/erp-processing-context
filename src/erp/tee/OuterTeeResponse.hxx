#ifndef ERP_PROCESSING_CONTEXT_TEE_OUTERTEERESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_TEE_OUTERTEERESPONSE_HXX

#include "erp/server/response/ServerResponse.hxx"

#include <string>


class SafeString;

class OuterTeeResponse
{
public:
    OuterTeeResponse (const std::string& a, const SafeString& aesKey);

    void convert (ServerResponse& response) const;

    std::string mEncryptedA;
};


#endif
