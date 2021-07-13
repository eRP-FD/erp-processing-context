#include "HttpsTestClient.hxx"

#include "erp/common/Constants.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/Expect.hxx"

#include "erp/util/Gsl.hxx"


std::unique_ptr< TestClient > HttpsTestClient::Factory()
{
    const std::string serverHost = Environment::getString("ERP_SERVER_HOST", defaultCloudServer);
    const int         serverPort = Environment::getInt("ERP_SERVER_PORT", defaultCloudPort);
    Expect(serverPort > 0 && serverPort <= 65535, "Environment variable ERP_SERVER_PORT is out of range");
    auto testClient = std::unique_ptr<TestClient>(
        new HttpsTestClient(serverHost, gsl::narrow<uint16_t>(serverPort), Constants::httpTimeoutInSeconds, false));
    VLOG(1) << "using: https://" << serverHost << ":" << serverPort;
    return testClient;
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

