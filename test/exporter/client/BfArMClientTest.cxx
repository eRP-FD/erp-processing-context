/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/BfArMClient.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/server/HttpsServer.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/network/client/CrlDownloadCache.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/server/RequestHandler.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <filesystem>

namespace
{
constexpr const char* hostIp = "127.0.0.1";
constexpr const char* tokenPath = "/ords/rezepte/oauth/token";

struct CertificateTestData {
    std::string rootCaCertPem;
    std::string rootCaCrlDer;
    std::string intermediateCertCrlDer;
    std::string leafPrivateKeyPem;
    std::string certChain;
};

CertificateTestData loadCert(const std::string& leafCert)
{
    const std::string rootCa = "root_ca_ec";
    const std::string intermediateCa = "sub_ca1_ec";
    const auto prefix = std::filesystem::path("test/generated_pki/");
    auto& resources = ResourceManager::instance();
    const auto intermediateCertPem = resources.getStringResource(
        (prefix / rootCa / "certificates" / intermediateCa / (intermediateCa + "_cert.pem")).string());
    const auto leafCertPem = resources.getStringResource(
        (prefix / intermediateCa / "certificates" / leafCert / (leafCert + "_cert.pem")).string());
    return CertificateTestData{
        .rootCaCertPem = resources.getStringResource((prefix / rootCa / "ca.pem").string()),
        .rootCaCrlDer = resources.getStringResource((prefix / rootCa / "crl/crl.der").string()),
        .intermediateCertCrlDer = resources.getStringResource((prefix / intermediateCa / "crl/crl.der").string()),
        .leafPrivateKeyPem = resources.getStringResource(
            (prefix / intermediateCa / "certificates" / leafCert / (leafCert + "_key.pem")).string()),
        .certChain = leafCertPem + intermediateCertPem,
    };
}

/// Answers the OAuth token request of the BfArMClient with a valid token response.
class TokenHandler : public UnconstrainedRequestHandler
{
public:
    void handleRequest(BaseSessionContext& session) override
    {
        session.response.setStatus(HttpStatus::OK);
        session.response.setBody(R"({"access_token":"token","expires_in":300,"token_type":"Bearer"})");
    }

    Operation getOperation() const override
    {
        return Operation::UNKNOWN;
    }
};

struct BfArMClientTestParam {
    std::string serverCert;
    bool connectionAccepted;
};
}// namespace


class BfArMClientTest : public testing::TestWithParam<BfArMClientTestParam>
{
protected:
    void TearDown() override
    {
        if (mServer)
        {
            mServer->shutDown();
            mServer.reset();
        }
    }

    void makeServer(const CertificateTestData& certData, uint16_t port)
    {
        const EnvironmentVariableGuard certEnv{ConfigurationKey::SERVER_CERTIFICATE, certData.certChain};
        const EnvironmentVariableGuard keyEnv{ConfigurationKey::SERVER_PRIVATE_KEY, certData.leafPrivateKeyPem};
        RequestHandlerManager handlerManager;
        handlerManager.onPostDo(tokenPath, std::make_unique<TokenHandler>());
        mServer = std::make_unique<exporter::HttpsServer>(hostIp, port, std::move(handlerManager), mServiceContext);
        mServer->serve(1, "test");
    }

private:
    EnvironmentVariableGuard mNoPostgres{TestConfigurationKey::TEST_USE_POSTGRES, "false"};
    boost::asio::io_context mIoContext;
    MedicationExporterFactories mFactories = MedicationExporterStaticData::makeMockMedicationExporterFactories();
    MedicationExporterServiceContext mServiceContext{mIoContext, Configuration::instance(), mFactories};
    std::unique_ptr<exporter::HttpsServer> mServer;
};


/**
 *  The BfArMClient must only accept a BfArM server certificate that has an EV or OV TLS certificate policy.
 */
TEST_P(BfArMClientTest, serverCertificatePolicy)
{
    A_28933.test("Validate certificate policy for BfArM server");
    const auto& param = GetParam();
    const auto port = gsl::narrow<uint16_t>(Configuration::instance().serverPort() + 13);
    const auto certData = loadCert(param.serverCert);
    makeServer(certData, port);

    const EnvironmentVariableGuard url{ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_URL, hostIp};
    const EnvironmentVariableGuard portGuard{ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_PORT,
                                             std::to_string(port)};
    const EnvironmentVariableGuard secret{ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_SECRET, "secret"};
    // trust the root CA of the test PKI, no other root CAs
    const EnvironmentVariableGuard internetCas{ConfigurationKey::INTERNET_TLS_ROOT_CA_PATH, ""};
    const EnvironmentVariableGuard trustedCas{ConfigurationKey::MEDICATION_EXPORTER_TRUSTED_CAS,
                                              "pem:" + certData.rootCaCertPem};

    // The client verifies the CRLs, the sender mock provides the ones of the test PKI.
    auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://crl.example.com/" + param.serverCert + ".crl", certData.intermediateCertCrlDer},
        {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});
    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);

    BfArMClient client{"clientId", crlProvider, "xContextId"};

    // testConnection() requests an access token and thus performs the TLS handshake with the server
    EXPECT_EQ(client.testConnection(), param.connectionAccepted);
}

INSTANTIATE_TEST_SUITE_P(certificatePolicies, BfArMClientTest,
                         testing::Values(BfArMClientTestParam{.serverCert = "ev_tls_ec", .connectionAccepted = true},
                                         BfArMClientTestParam{.serverCert = "ov_tls_ec", .connectionAccepted = true},
                                         // valid certificate, but neither EV nor OV policy
                                         BfArMClientTestParam{.serverCert = "unrevoked_ec",
                                                              .connectionAccepted = false}),
                         [](const testing::TestParamInfo<BfArMClientTestParam>& info) {
                             return info.param.serverCert;
                         });
