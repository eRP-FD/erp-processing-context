/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Base64.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"


class IdpUpdaterTest : public testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    std::shared_ptr<TslManager> createAndSetupTslManager (
        const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& additionalOcspResponderKnownCertificateCaPairs = {})
    {
        auto idpCertificateCa = Certificate::fromPem(
            FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

        std::map<std::string, const std::vector<MockOcsp::CertificatePair>> ocspResponderKnownCertificateCaPairs
            = {
                  {"http://ocsp-testref.tsl.telematik-test/ocsp",
                      {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                          {*mIdpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}
            };
        ocspResponderKnownCertificateCaPairs.insert(
            additionalOcspResponderKnownCertificateCaPairs.begin(),
            additionalOcspResponderKnownCertificateCaPairs.end());

        auto tslManager = TslTestHelper::createTslManager<TslManager>(
            {},
            {},
            ocspResponderKnownCertificateCaPairs);
        std::weak_ptr<TslManager> mgrWeakPtr{tslManager};
        tslManager->addPostUpdateHook([mgrWeakPtr, idpCertificateCa] {
            auto tslManager = mgrWeakPtr.lock();
            if (! tslManager)
                return;
            TslTestHelper::addCaCertificateToTrustStore(idpCertificateCa, *tslManager, TslMode::TSL);
        });

        return tslManager;
    }

    IdpUpdaterTest()
        : mIoThread{[this]() {
            auto workGuard = boost::asio::make_work_guard(io);
            io.run();
        }}
    {
    }

    ~IdpUpdaterTest() override
    {
        io.stop();
        if (mIoThread.joinable())
        {
            mIoThread.join();
        }
    }

    void SetUp() override
    {
        auto& resMgr = ResourceManager::instance();
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        mIdpCertificate.emplace(Certificate::fromPem(
            resMgr.getStringResource("test/tsl/X509Certificate/IDP-Wansim.pem")));

        mIdpResponseJson = resMgr.getStringResource("test/tsl/X509Certificate/idpResponse.json");
        mIdpResponseJson = std::regex_replace(mIdpResponseJson, std::regex{"###CERTIFICATE##"},
                                         mIdpCertificate.value().toBase64Der());

    }

    void TearDown() override
    {
        mCaDerPathGuard.reset();
    }

    std::optional<Certificate> mIdpCertificate;
    std::string mIdpResponseJson;
    boost::asio::io_context io;
    std::thread mIoThread;
};

namespace
{

/**
 * Avoid the download of the IDP signer certificate so that tests can work without internet connection or when the
 * discovery server is down.
 */
class IdpTestUpdater : public IdpUpdater
{
public:
    IdpTestUpdater (Idp& certificateHolder, TslManager& tslManager, boost::asio::io_context& io)
        : IdpUpdater(certificateHolder, tslManager, {}, io)
    {
    }

    size_t updateCount = 0;

protected:
    std::string doDownloadWellknown (void) override
    {
        return "";
    }
    UrlHelper::UrlParts doParseWellknown (std::string&&) override
    {
        ++updateCount;
        return {"","",443,"",""};
    }
    std::string doDownloadDiscovery (const UrlHelper::UrlParts&) override
    {
        return "";
    }
    std::vector<Certificate> doParseDiscovery (std::string&&) override
    {
        return {StaticData::idpCertificate};
    }
};


/**
 * Simulate errors in individual steps of the IDP signer certificate update.
 */
class IdpErrorUpdater : public IdpUpdater
{
public:
    IdpErrorUpdater(Idp& certificateHolder, TslManager& tslManager, const UpdateStatus mockStatus,
                    boost::asio::io_context& io)
        : IdpUpdater(certificateHolder, tslManager, {}, io)
        , mMockStatus(mockStatus)
    {
    }

    void setMockedUpdateStatus (const UpdateStatus mockStatus)
    {
        mMockStatus = mockStatus;
    }

    using UpdateStatus = IdpUpdater::UpdateStatus; // Make it accessible to tests.

protected:
    std::string doDownloadWellknown (void) override
    {
        return "";
    }
    UrlHelper::UrlParts doParseWellknown (std::string&&) override
    {
        if (mMockStatus == UpdateStatus::WellknownDownloadFailed)
            throw std::runtime_error("test");
        else
            return {"","",443,"",""};
    }
    std::string doDownloadDiscovery (const UrlHelper::UrlParts&) override
    {
        return "";
    }
    std::vector<Certificate> doParseDiscovery (std::string&&) override
    {
        if (mMockStatus == UpdateStatus::DiscoveryDownloadFailed)
            throw std::runtime_error("test");
        else
            return {StaticData::idpCertificate};
    }
    void doVerifyCertificate (const std::vector<Certificate>&) override
    {
        if (mMockStatus == UpdateStatus::VerificationFailed)
            throw std::runtime_error("test");
    }
    void reportUpdateStatus (UpdateStatus status, std::string_view details) override
    {
        ASSERT_EQ(status, mMockStatus);

        IdpUpdater::reportUpdateStatus(status, details);
    }

private:
    UpdateStatus mMockStatus;
};


class IdpBrokenCertificateUpdater : public IdpUpdater
{
public:
    IdpBrokenCertificateUpdater(Idp& certificateHolder, TslManager& tslManager,
                                const std::shared_ptr<UrlRequestSender>& urlRequestSender, boost::asio::io_context& io)
        : IdpUpdater(
              certificateHolder, tslManager, urlRequestSender,
              io)// NOLINT(performance-unnecessary-value-param) - have to check if it would have side-effets in the upper layer.
    {
    }

    std::vector<Certificate> doParseDiscovery (std::string&& jwtString) override
    {
        try
        {
            return IdpUpdater::doParseDiscovery(std::move(jwtString));
        }
        catch(const TslError& e)
        {
            for (const auto& errorData : e.getErrorData())
            {
                tslErrorCodes.emplace_back(errorData.errorCode);
            }
            throw;
        }
    }

    std::vector<TslErrorCode> tslErrorCodes;
};

} // end of anonymous namespace


/**
 * Use the production IdpUpdater to connect to the "real" (but can be configured to use a mocked remote) server.
 */
TEST_F(IdpUpdaterTest, update) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", mIdpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * Simulate response with broken IDP certificate.
 */
TEST_F(IdpUpdaterTest, updateWithBrokenResponse) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJson = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpBrokenResponse.json");
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    auto updater = IdpUpdater::create<IdpBrokenCertificateUpdater>(
        idp,
        *tslManager,
        io,
        false,
        idpRequestSender);

    updater->update();

    ASSERT_EQ(1, updater->tslErrorCodes.size());
    ASSERT_EQ(TslErrorCode::CERT_READ_ERROR, updater->tslErrorCodes[0]);

    // The certificate is still not set.
    ASSERT_ANY_THROW(idp.getCertificate());
}


/**
 * Simulate failed update attempts for the maximum age of the certificate. This is 24 hours in production.
 */
TEST_F(IdpUpdaterTest, DISABLED_update_resetForMaxAge) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Simulate an earlier successful update.
    idp.setCertificate(Certificate(StaticData::idpCertificate));
    ASSERT_NO_THROW(idp.getCertificate());

    // A missing successful update, i.e. directly after the application starts, is treated like it was
    // on the start of the epoch. That means it is more than 24 hours in the past and as a result the IDP
    // certificate is reset.
    IdpErrorUpdater updater (idp, *tslManager, IdpErrorUpdater::UpdateStatus::WellknownDownloadFailed, io);
    updater.update();

    // The failed update is expected to trigger the 24 hours maximum age of the certificate.
    ASSERT_ANY_THROW(idp.getCertificate());
}


/**
 * Simulate a failed update attempt. But this time the last successful attempt is not 24 hours in the past.
 */
TEST_F(IdpUpdaterTest, update_noResetForMaxAge) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Simulate an earlier successful update.
    idp.setCertificate(Certificate(StaticData::idpCertificate));
    ASSERT_NO_THROW(idp.getCertificate());

    // One successful update to set the lastSuccessfulUpdateTime to "now".
    IdpErrorUpdater updater (idp, *tslManager, IdpErrorUpdater::UpdateStatus::Success, io);
    updater.update();

    // Now a failed one. As the last successful update is not 2h hours in the past the IDP certificate is not reset...
    updater.setMockedUpdateStatus(IdpErrorUpdater::UpdateStatus::WellknownDownloadFailed);
    updater.update();

    // ... and should still be accessible.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * Use the a mocked updater to use a static version of the IDP signer certificate.
 */
TEST_F(IdpUpdaterTest, updateStaticCertificate) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", mIdpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    updater->update();

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * This set of static data comes from https://dth01.ibmgcloud.net/jira/browse/ERP-5948
 */
TEST_F(IdpUpdaterTest, updateStaticCertificate2) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto idpCertificateErp5948 = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/erp-5948-certificate.pem"));
    auto idpCertificateGemKompCa10 = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-GEM.KOMP-CA10.pem"));
    auto tslManager = createAndSetupTslManager(
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/ecc-ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                 {idpCertificateErp5948, idpCertificateGemKompCa10, MockOcsp::CertificateOcspTestMode::SUCCESS}}}
        });

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // Prepare the "download" of static files.
    const std::string idpResponseJson = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/erp-5948-puk_idp_sig.json");
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/erp-5948-openid-configuration");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/ti/certs/puk_idp_sig.json", idpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}
        });

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    updater->update();

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


TEST_F(IdpUpdaterTest, update_failForWellknownConfigurationFailed)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();
    IdpErrorUpdater(idp, *tslManager, IdpErrorUpdater::UpdateStatus::WellknownDownloadFailed, io)
        .update();
}


TEST_F(IdpUpdaterTest, update_failForDiscoveryDownloadFailed)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();
    IdpErrorUpdater(idp, *tslManager, IdpErrorUpdater::UpdateStatus::DiscoveryDownloadFailed, io)
        .update();
}


// Disabled while failed verifications are handled as warnings.
TEST_F(IdpUpdaterTest, DISABLED_update_failForVerificationFailed)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();
    IdpErrorUpdater(idp, *tslManager, IdpErrorUpdater::UpdateStatus::VerificationFailed, io)
        .update();
}


/**
 * Simulate an update of the TslManager and verify that that triggers an update of the IDP updater.
 *
 * Disabled to figure out how to reliably force a Tsl update.
 */
TEST_F(IdpUpdaterTest, DISABLED_IdpUpdateAfterTslUpdate)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    IdpTestUpdater updater (idp, *tslManager, io);

    // The IdpTestUpdater does not update in its constructor.
    ASSERT_EQ(updater.updateCount, 0);

    // Trigger an update of the TslManager ...
    tslManager->updateTrustStoresOnDemand();

    // ... and verify that the IdpUpdater has also been called.
    ASSERT_EQ(updater.updateCount, 1);
}

/**
 * Simulate an unstable IDP endpoint where at least 2 retries are required to get a valid response
 */
TEST_F(IdpUpdaterTest, initializeWithForcedRetries) // NOLINT(readability-function-cognitive-complexity)
{
    // instead of the LU configuration use the prod config which has the advantage that
    // it returns more than one DNS entry which are tried one after another then
    EnvironmentVariableGuard idpEndpoint(
        ConfigurationKey::IDP_UPDATE_ENDPOINT,
        "https://idp.zentral.idp.splitdns.ti-dienste.de/.well-known/openid-configuration");
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", mIdpResponseJson}});

    idpRequestSender->setUrlHandler("https://idp.zentral.idp.splitdns.ti-dienste.de:443/.well-known/openid-configuration",
                                    [&idpResponseJwk](const std::string&) -> ClientResponse
                                    {
                                        static int count=0;
                                        Header header;
                                        if (count++ < 2 )
                                        {
                                            header.setStatus(HttpStatus::NetworkConnectTimeoutError);
                                            header.setContentLength(0);
                                            return {header, ""};
                                        }
                                        else
                                        {
                                            header.setStatus(HttpStatus::OK);
                                            header.setContentLength(idpResponseJwk.size());
                                            return {header, idpResponseJwk};
                                        }
                                    });

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * Simulate an unstable IDP endpoint that never returns a valid response
 */
TEST_F(IdpUpdaterTest, initializeFailedWithForcedRetries) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJson = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponse.json");
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson}});

    idpRequestSender->setUrlHandler("https://idp.lu.erezepttest.net:443/.well-known/openid-configuration",
                                    [](const std::string&) -> ClientResponse
                                    {
                                      Header header;
                                      header.setStatus(HttpStatus::NetworkConnectTimeoutError);
                                      header.setContentLength(0);
                                      return {header, ""};
                                    });

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    // ... still no certificate is set due to failing download.
    ASSERT_ANY_THROW(idp.getCertificate());
}

/**
 * Simulate an unstable IDP endpoint that never returns a valid response
 * Afterwards fix the IDP endpoint,
 * The IDP should recover in IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS in this case
 */
TEST_F(IdpUpdaterTest, secondaryTimerTest) // NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // After the initial update ...
    const std::string idpResponseJson = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponse.json");
    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson}});

    idpRequestSender->setUrlHandler("https://idp.lu.erezepttest.net:443/.well-known/openid-configuration",
                                    [](const std::string&) -> ClientResponse
                                    {
                                        Header header;
                                        header.setStatus(HttpStatus::NetworkConnectTimeoutError);
                                        header.setContentLength(0);
                                        return {header, ""};
                                    });

    EnvironmentVariableGuard guard(ConfigurationKey::IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS, "1");

    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    // ... still no certificate is set due to failing download.
    ASSERT_ANY_THROW(idp.getCertificate());


    // fix the download
    idpRequestSender->setResponse("https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", mIdpResponseJson);
    idpRequestSender->setResponse("https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}
