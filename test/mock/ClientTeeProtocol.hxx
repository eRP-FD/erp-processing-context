#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_CLIENTTEEPROTOCOL_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_CLIENTTEEPROTOCOL_HXX


#include <erp/client/response/ClientResponse.hxx>
#include "erp/client/request/ClientRequest.hxx"

#include "erp/util/SafeString.hxx"

#include "erp/crypto/OpenSslHelper.hxx"


class Certificate;
class JWT;


/**
 * Client side of the TEE protocol.
 * This implementation will not be part of the production build but is required for testing.
 */
class ClientTeeProtocol
{
public:
    /**
     * Create a new instance with a randomly created ephemeral key and iv.
     */
    ClientTeeProtocol (void);

    void setEphemeralKeyPair (const shared_EVP_PKEY& keyPair);
    void setIv (SafeString&& iv);

    std::string createRequest (
        const Certificate& serverPublicKeyCertificate,
        const ClientRequest& request,
        const JWT& jwt);
    std::string createRequest (
        const Certificate& serverPublicKeyCertificate,
        const std::string& requestContent,
        const JWT& jwt);
    std::string createRequest (
        const Certificate& serverPublicKeyCertificate,
        const ClientRequest& request,
        const std::string& jwt);
    std::string createRequest (
        const Certificate& serverPublicKeyCertificate,
        const std::string& requestContent,
        const std::string& jwt);
    std::string encryptInnerRequest (
        const Certificate& serverPublicKeyCertificate,
        const SafeString& p);


    ClientResponse parseResponse (
        const ClientResponse& response);

private:
    shared_EVP_PKEY mEphemeralKeyPair;
    SafeString mIv;
    std::string mRequestIdInRequest;
    SafeString mResponseSymmetricKey;
};


#endif
