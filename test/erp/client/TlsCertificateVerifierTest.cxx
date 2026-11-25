/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "shared/network/client/CrlDownloadCache.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/server/RequestHandler.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <filesystem>
#include <iterator>


class TestServerHandler : public UnconstrainedRequestHandler
{
public:
    void handleRequest(BaseSessionContext& session) override
    {
        session.response.setStatus(HttpStatus::OK);
        session.response.setBody("");
    }

    Operation getOperation() const override
    {
        return Operation::UNKNOWN;
    }
};

struct CertificateTestData {
    std::filesystem::path rootCaCertFile;
    std::string rootCaCertPem;
    std::string rootCaCrlDer;
    std::string intermediateCertPem;
    std::string intermediateCertCrlDer;
    std::string leafCertPem;
    std::string leafPrivateKeyPem;
    std::string certChain;
};

CertificateTestData loadCert(std::string rootCa, std::string intermediateCa, std::string leafCert)
{
    const auto prefix = std::filesystem::path("test/generated_pki/");
    auto rootCaCertFile = (prefix / rootCa / "ca.pem");
    const auto rootCaPem = ResourceManager::instance().getStringResource(rootCaCertFile.string());
    const auto rootCaCrlDer = ResourceManager::instance().getStringResource((prefix / rootCa / "crl/crl.der").string());
    const auto intermediateCertPem = ResourceManager::instance().getStringResource(
        (prefix / rootCa / "certificates" / intermediateCa / (intermediateCa + "_cert.pem")).string());
    const auto intermediateCertCrlDer =
        ResourceManager::instance().getStringResource((prefix / intermediateCa / "crl/crl.der").string());
    const auto leafCertPem = ResourceManager::instance().getStringResource(
        (prefix / intermediateCa / "certificates" / leafCert / (leafCert + "_cert.pem")).string());
    const auto leafPrivateKeyPem = ResourceManager::instance().getStringResource(
        (prefix / intermediateCa / "certificates" / leafCert / (leafCert + "_key.pem")).string());
    return CertificateTestData{
        .rootCaCertFile = std::move(rootCaCertFile),
        .rootCaCertPem = rootCaPem,
        .rootCaCrlDer = rootCaCrlDer,
        .intermediateCertPem = intermediateCertPem,
        .intermediateCertCrlDer = intermediateCertCrlDer,
        .leafCertPem = leafCertPem,
        .leafPrivateKeyPem = leafPrivateKeyPem,
        .certChain = leafCertPem + intermediateCertPem,
    };
}

class TlsCertificateVerifierTest : public testing::Test
{
protected:
    static constexpr const char* hostIp = "127.0.0.1";
    std::uint16_t port = Configuration::instance().serverPort() + 11;
    std::unique_ptr<HttpsServer> server;
    std::string requestPath;

    void SetUp() override
    {
        std::stringstream url;
        url << "https://127.0.0.1:" << port << "/test_path";
        requestPath = url.str();
    }

    void makeServer(const std::string& certificate, const std::string& privateKey)
    {
        const EnvironmentVariableGuard certEnv{ConfigurationKey::SERVER_CERTIFICATE, certificate};
        const EnvironmentVariableGuard keyEnv{ConfigurationKey::SERVER_PRIVATE_KEY, privateKey};
        RequestHandlerManager handlerManager;
        std::unique_ptr<RequestHandlerInterface> requestHandler = std::make_unique<TestServerHandler>();
        handlerManager.onPostDo("/test_path", std::move(requestHandler));
        server = std::make_unique<HttpsServer>(hostIp, port, std::move(handlerManager), serviceContext);
        server->serve(1, "test");
    }

    void TearDown() override
    {
        if (server != nullptr)
        {
            server->shutDown();
            server.reset();
        }
    }

private:
    PcServiceContext serviceContext = StaticData::makePcServiceContext();
};


/**
 * Normal certificate with no entry in the crl
 */
TEST_F(TlsCertificateVerifierTest, crlHappyPath)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);

    for (const auto& mode : {TlsCertificateVerifier::CrlMode::HARD_FAIL, TlsCertificateVerifier::CrlMode::SOFT_FAIL})
    {
        TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
        const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

        EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

        auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
            {"http://crl.example.com/unrevoked_ec.crl", certData.intermediateCertCrlDer},
            {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

        auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
        verifier.withCrl(*crlProvider, mode);

        EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
    }
}

/**
 * Test that a revoked leaf certificate causes the crl validation to fail
 */
TEST_F(TlsCertificateVerifierTest, crlRevokedLeaf)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "revoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);

    for (const auto& mode : {TlsCertificateVerifier::CrlMode::HARD_FAIL, TlsCertificateVerifier::CrlMode::SOFT_FAIL})
    {
        TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
        const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

        EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

        auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
            {"http://crl.example.com/revoked_crt.crl", certData.intermediateCertCrlDer},
            {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

        auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
        verifier.withCrl(*crlProvider, mode);

        EXPECT_ANY_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
    }
}

/**
 * Test that a revoked intermediate certificate causes the crl check to fail
 */
TEST_F(TlsCertificateVerifierTest, crlRevokedIntermediate)
{
    auto certData = loadCert("root_ca_ec", "revoked_ca_ec", "normal_ec");

    makeServer(certData.certChain, certData.leafPrivateKeyPem);

    for (const auto& mode : {TlsCertificateVerifier::CrlMode::HARD_FAIL, TlsCertificateVerifier::CrlMode::SOFT_FAIL})
    {
        TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
        const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

        EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

        auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
            {"http://crl.example.com/normal_ec.crl", certData.intermediateCertCrlDer},
            {"http://crl.example.com/revoked_ca_ec.crl", certData.rootCaCrlDer}});

        auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
        verifier.withCrl(*crlProvider, mode);


        EXPECT_ANY_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
    }
}


/**
 * Test that an invalid signer for the intermediate causes a fail
 */
TEST_F(TlsCertificateVerifierTest, crlInvalidSigner)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);

    for (const auto& mode : {TlsCertificateVerifier::CrlMode::HARD_FAIL, TlsCertificateVerifier::CrlMode::SOFT_FAIL})
    {
        TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
        const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

        EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

        auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
            {"http://crl.example.com/unrevoked_ec.crl", certData.intermediateCertCrlDer},
            {"http://crl.example.com/sub_ca1_ec.crl", certData.intermediateCertCrlDer}});

        auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
        verifier.withCrl(*crlProvider, mode);

        EXPECT_ANY_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
    }
}

TEST_F(TlsCertificateVerifierTest, crlHardFailNoConnection)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);
    TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
    const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

    auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

    crlRequestSender->setUrlHandler("http://crl.example.com/unrevoked_ec.crl", [](const auto&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::NotFound);
        return {header, ""};
    });

    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
    verifier.withCrl(*crlProvider, TlsCertificateVerifier::CrlMode::HARD_FAIL);

    EXPECT_ANY_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
}


TEST_F(TlsCertificateVerifierTest, crlSoftFail404)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);
    TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
    const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

    auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

    crlRequestSender->setUrlHandler("http://crl.example.com/unrevoked_ec.crl", [](const auto&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::NotFound);
        return {header, ""};
    });

    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
    verifier.withCrl(*crlProvider, TlsCertificateVerifier::CrlMode::SOFT_FAIL);

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
}


TEST_F(TlsCertificateVerifierTest, crlSoftFailEmpty)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);
    TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
    const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

    auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

    crlRequestSender->setUrlHandler("http://crl.example.com/unrevoked_ec.crl", [](const auto&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::OK);
        return {header, ""};
    });

    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
    verifier.withCrl(*crlProvider, TlsCertificateVerifier::CrlMode::SOFT_FAIL);

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
}


TEST_F(TlsCertificateVerifierTest, crlHardFailEmpty)
{
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", "unrevoked_ec");
    makeServer(certData.certChain, certData.leafPrivateKeyPem);
    TlsCertificateVerifier verifier = TlsCertificateVerifier::withCustomRootCertificates(certData.rootCaCertPem);
    const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});

    EXPECT_NO_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));

    auto crlRequestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://crl.example.com/sub_ca1_ec.crl", certData.rootCaCrlDer}});

    crlRequestSender->setUrlHandler("http://crl.example.com/unrevoked_ec.crl", [](const auto&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::OK);
        return {header, ""};
    });

    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
    verifier.withCrl(*crlProvider, TlsCertificateVerifier::CrlMode::HARD_FAIL);

    EXPECT_ANY_THROW(requestSender.send(requestPath, HttpMethod::GET, ""));
}

struct TlsInternetCertificateVerifierTestParam {
    enum CertRepresentation
    {
        NONE,
        PEM,
        FILE,
        OTHER_CA_FILE,
    };
    struct ConfVal {
        std::string prefix{};
        CertRepresentation certificateRep = NONE;
    };
    bool success;
    ConfVal conf{};
    ConfVal fallback{};
    std::string serverCert{"unrevoked_ec"};

};


class TlsInternetCertificateVerifierTest : public TlsCertificateVerifierTest,
                                           public testing::WithParamInterface<TlsInternetCertificateVerifierTestParam>
{
public:
    static constexpr auto internetKey = ConfigurationKey::INTERNET_TLS_ROOT_CA_PATH;
    static constexpr auto fallbackKey = ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_SERVER_CA;

    std::string confValue(const CertificateTestData& certData, const TlsInternetCertificateVerifierTestParam::ConfVal& cv)
    {
        std::string result;
        switch (cv.certificateRep)
        {
            case TlsInternetCertificateVerifierTestParam::NONE:
                return cv.prefix;
            case TlsInternetCertificateVerifierTestParam::PEM:
                return cv.prefix + certData.rootCaCertPem;
            case TlsInternetCertificateVerifierTestParam::FILE:
                return cv.prefix + ResourceManager::getAbsoluteFilename(certData.rootCaCertFile).native();
            case TlsInternetCertificateVerifierTestParam::OTHER_CA_FILE:
                return cv.prefix +
                       ResourceManager::getAbsoluteFilename("test/generated_pki_push/rootCA/rootCA-cert.pem").native();
        }
        Fail2("Invalid value for CertRepresentation: " + std::to_string(static_cast<uintmax_t>(cv.certificateRep)),
              std::logic_error);
    }

    void test()
    {
        auto verifier = TlsCertificateVerifier::withInternetRootCAsWithFallback(fallbackKey);
        const UrlRequestSender requestSender(verifier, std::chrono::seconds{1}, std::chrono::seconds{1});
        requestSender.send(requestPath, HttpMethod::GET, "");
    }
};
void PrintTo(const TlsInternetCertificateVerifierTestParam& p, std::ostream* out)
{
    fmt::format_to(std::ostream_iterator<char>{*out}, "conf: {}{}, fallback: {}{}, server: {}", p.conf.prefix,
                    magic_enum::enum_name(p.conf.certificateRep), p.fallback.prefix,
                    magic_enum::enum_name(p.fallback.certificateRep), p.serverCert);
}

TEST_P(TlsInternetCertificateVerifierTest, internetCert)
{
    const auto& param = GetParam();
    auto certData = loadCert("root_ca_ec", "sub_ca1_ec", param.serverCert);
    makeServer(certData.certChain, certData.leafPrivateKeyPem);
    EnvironmentVariableGuard caFile{internetKey, confValue(certData, param.conf)};
    EnvironmentVariableGuard noFallback{fallbackKey, confValue(certData, param.fallback)};
    if (param.success)
    {
        ASSERT_NO_THROW(test());
    }
    else
    {
        ASSERT_ANY_THROW(test());
    }
}

INSTANTIATE_TEST_SUITE_P(success, TlsInternetCertificateVerifierTest,
                         testing::ValuesIn<std::list<TlsInternetCertificateVerifierTestParam>>({
                             {
                                 .success = true,
                                 .conf = {"", TlsInternetCertificateVerifierTestParam::FILE},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = true,
                                 .fallback = {"file://", TlsInternetCertificateVerifierTestParam::FILE},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = true,
                                 .fallback = {"pem:", TlsInternetCertificateVerifierTestParam::PEM},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = true,
                                 .fallback = {"", TlsInternetCertificateVerifierTestParam::FILE},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = true,
                                 .fallback = {"", TlsInternetCertificateVerifierTestParam::PEM},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = true,
                                 .conf = {"", TlsInternetCertificateVerifierTestParam::FILE},
                                 .fallback = {"<broken fallback argument>"},
                                 .serverCert = "unrevoked_ec",
                             },
                         }));


INSTANTIATE_TEST_SUITE_P(failure, TlsInternetCertificateVerifierTest,
                         testing::ValuesIn<std::list<TlsInternetCertificateVerifierTestParam>>({
                             {
                                 .success = false,
                                 .conf = {},
                                 .fallback = {},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = false,
                                 .conf = {"", TlsInternetCertificateVerifierTestParam::OTHER_CA_FILE},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = false,
                                 .fallback = {"file://", TlsInternetCertificateVerifierTestParam::OTHER_CA_FILE},
                                 .serverCert = "unrevoked_ec",
                             },
                             {
                                 .success = false,
                                 .conf = {"", TlsInternetCertificateVerifierTestParam::FILE},
                                 .serverCert = "outdated_ec",
                             },
                             {
                                 .success = false,
                                 .fallback = {"file://", TlsInternetCertificateVerifierTestParam::FILE},
                                 .serverCert = "outdated_ec",
                             },
                             {
                                 .success = false,
                                 .fallback = {"<broken fallback argument>"},
                                 .serverCert = "unrevoked_ec",
                             }
                         }));

