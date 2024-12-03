/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "HttpsTestClient.hxx"

#include "shared/common/Constants.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "mock/crypto/MockCryptography.hxx"


std::unique_ptr<TestClient> HttpsTestClient::factory(std::shared_ptr<XmlValidator>, Target target) //NOLINT[performance-unnecessary-value-param]
{
    const std::string serverHost = Environment::getString("ERP_SERVER_HOST", defaultCloudServer);
    uint16_t serverPort = getTargetPort(target);
    Expect(serverPort > 0, "Environment variable ERP_SERVER_PORT is out of range");
    std::unique_ptr<HttpsTestClient> testClient{
        new HttpsTestClient(ConnectionParameters{
            .hostname = serverHost,
            .port = std::to_string(serverPort),
            .connectionTimeoutSeconds = Constants::httpTimeoutInSeconds,
            .resolveTimeout = Constants::resolveTimeout,
            .tlsParameters = TlsConnectionParameters{
                .certificateVerifier = TlsCertificateVerifier::withVerificationDisabledForTesting()
            }
        })};
    TVLOG(1) << "using: https://" << serverHost << ":" << serverPort;
    testClient->mRemoteCertificate = testClient->retrieveEciesRemoteCertificate();
    return testClient;
}

HttpsTestClient::~HttpsTestClient()
{
    try
    {
        mHttpsClient.close();
    }
    catch (const std::exception& ex)
    {
        TLOG(WARNING) << "HttpsTestClient shutdown failed: " << ex.what();
    }
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
              {Header::UserAgent, "eRp-Testsuite/1.0.0 GMTIK/JMeter-Internet"s},
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

uint16_t HttpsTestClient::getTargetPort(TestClient::Target target)
{
    const auto& config = Configuration::instance();
    switch (target)
    {
        case Target::ADMIN:
            return gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT));
        case Target::VAU:
            return config.serverPort();
        case Target::ENROLMENT:
            return gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ENROLMENT_SERVER_PORT));
    }
    Fail2("Invalid value for target: " + std::to_string(static_cast<intmax_t>(target)), std::logic_error);
}

Certificate HttpsTestClient::getEciesCertificate(void)
{
    if (mRemoteCertificate)
    {
        return *mRemoteCertificate;
    }
    return TestClient::getEciesCertificate();
}

bool HttpsTestClient::runsInErpTest() const
{
    return false;
}


HttpsTestClient::HttpsTestClient(const ConnectionParameters& params)
    : mHttpsClient(params)
    , mServerAddress(params.hostname)
    , mPort(gsl::narrow<uint16_t>(std::stoi(params.port)))
{
}


ClientResponse HttpsTestClient::send(const ClientRequest& clientRequest)
{
    return mHttpsClient.send(clientRequest);
}
