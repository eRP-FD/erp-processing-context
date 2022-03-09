/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "HttpsTestClient.hxx"

#include "erp/common/Constants.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Gsl.hxx"
#include "mock/crypto/MockCryptography.hxx"


std::unique_ptr< TestClient > HttpsTestClient::Factory(std::shared_ptr<XmlValidator>)
{
    const std::string serverHost = Environment::getString("ERP_SERVER_HOST", defaultCloudServer);
    const int         serverPort = Environment::getInt("ERP_SERVER_PORT", defaultCloudPort);
    Expect(serverPort > 0 && serverPort <= 65535, "Environment variable ERP_SERVER_PORT is out of range");
    std::unique_ptr<HttpsTestClient> testClient{
        new HttpsTestClient(serverHost, gsl::narrow<uint16_t>(serverPort), Constants::httpTimeoutInSeconds, false)};
    VLOG(1) << "using: https://" << serverHost << ":" << serverPort;
    testClient->mRemoteCertificate = testClient->retrieveEciesRemoteCertificate();
    return testClient;
}

HttpsTestClient::~HttpsTestClient()
try
{
    mHttpsClient.close();
}
catch (const std::exception& ex)
{
    TLOG(WARNING) << "HttpsTestClient shutdown failed: " << ex.what();
}


std::string HttpsTestClient::getHostHttpHeader() const
{
    using std::to_string;
    return mServerAddress + ':' + to_string(mPort);
}


std::string HttpsTestClient::getHostAddress() const
{
    return mServerAddress;
}


uint16_t HttpsTestClient::getPort() const
{
    return mPort;
}


std::unique_ptr<Certificate> HttpsTestClient::retrieveEciesRemoteCertificate()
{
    using namespace std::string_literals;
    std::unique_ptr<Certificate> result;
    ClientRequest getVAUCertificateRequest(Header(HttpMethod::GET,
            "/VAUCertificate",
            Header::Version_1_1,
            { {Header::Host, getHostHttpHeader() },
              {Header::UserAgent, "vau-cpp-it-test"s},
              {Header::Connection, Header::ConnectionClose}
            },
            HttpStatus::Unknown), std::string{});
    static bool certificateRequested = false;
    static std::string remoteCertificate;
    if (!certificateRequested)
    {
        try {
            const auto& response = send(getVAUCertificateRequest);
            Expect(response.getHeader().status() == HttpStatus::OK,
                "Response status not OK: "s.append(magic_enum::enum_name(response.getHeader().status())));
            remoteCertificate = response.getBody();
        }
        catch (const std::runtime_error& re)
        {
            TVLOG(0) << "could not retrieve remoteCertificate: " << re.what() << " - using default";
        }
    }
    if (!remoteCertificate.empty())
    {
        result = std::make_unique<Certificate>(Certificate::fromBinaryDer(remoteCertificate));
        TVLOG(0) << "Using remote Certificate from: " + getHostAddress();
        return result;
    }
    return nullptr;
}


Certificate HttpsTestClient::getEciesCertificate (void)
{
    if (mRemoteCertificate)
    {
        return *mRemoteCertificate;
    }
    return TestClient::getEciesCertificate();
}


HttpsTestClient::HttpsTestClient(
    const std::string& host,
    uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey)
    : mHttpsClient(host, port, connectionTimeoutSeconds, enforceServerAuthentication, 
                   caCertificates, clientCertificate, clientPrivateKey)
    , mServerAddress(host)
    , mPort(port)
{
}


ClientResponse HttpsTestClient::send(const ClientRequest& clientRequest)
{
    return mHttpsClient.send(clientRequest);
}

