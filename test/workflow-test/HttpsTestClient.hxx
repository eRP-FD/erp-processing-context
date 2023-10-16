/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_HTTPSTESTCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TEST_HTTPSTESTCLIENT_HXX

#include "erp/client/HttpsClient.hxx"
#include "TestClient.hxx"

static constexpr auto defaultCloudServer { "localhost" };           // alternative names: erp.box.erezepttest.net, erp.lu.erezepttest.net, localhost

class HttpsTestClient
    : public TestClient
{
public:
    static std::unique_ptr<TestClient> factory(std::shared_ptr<XmlValidator> xmlValidator, Target target);

    ~HttpsTestClient() override;

    std::string getHostHttpHeader() const override;

    std::string getHostAddress() const override;

    uint16_t getPort() const override;

    Certificate getEciesCertificate() override;

protected:
    HttpsTestClient(
        const std::string& host,
        std::uint16_t port,
        const uint16_t connectionTimeoutSeconds,
        std::chrono::milliseconds resolveTimeout,
        bool enforceServerAuthentication = true,
        const SafeString& caCertificates = SafeString(),
        const SafeString& clientCertificate = SafeString(),
        const SafeString& clientPrivateKey = SafeString());
    ClientResponse send(const ClientRequest & clientRequest) override;
private:
    std::unique_ptr<Certificate> retrieveEciesRemoteCertificate();
    static uint16_t getTargetPort(Target target);

    HttpsClient mHttpsClient;
    std::string const mServerAddress;
    uint16_t mPort;
    std::unique_ptr<Certificate> mRemoteCertificate;
};


#endif // ERP_PROCESSING_CONTEXT_TEST_HTTPSTESTCLIENT_HXX
