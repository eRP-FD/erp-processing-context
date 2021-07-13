#ifndef TEST_ERP_PROCESSING_CONTEXT_UT_TSL_MOCKOCSPSERVER_HXX
#define TEST_ERP_PROCESSING_CONTEXT_UT_TSL_MOCKOCSPSERVER_HXX

#include "erp/server/HttpsServer.hxx"
#include "erp/util/Configuration.hxx"
#include "test/mock/MockOcsp.hxx"

#include <memory>


class OcspServiceContext
{
};

class MockOcspServer
{
public:
    /// Factory method to create an HttpsServer that will handle incoming signing requests
    static std::unique_ptr<HttpsServer<OcspServiceContext>> create (
        const std::string& hostIp,
        uint16_t port,
        const std::vector<MockOcsp::CertificatePair> ocspResponderKnownCertificateCaPairs);
};


#endif
