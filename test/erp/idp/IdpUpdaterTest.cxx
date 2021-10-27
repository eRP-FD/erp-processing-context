/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Base64.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockOcsp.hxx"
#include "test/mock/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"


class IdpUpdaterTest : public testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    std::shared_ptr<TslManager> createAndSetupTslManager (
        const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& additionalOcspResponderKnownCertificateCaPairs = {})
    {
        auto idpCertificate = Certificate::fromDerBase64String(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.base64.der"));
        auto idpCertificateCa = Certificate::fromDerBase64String(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

        std::map<std::string, const std::vector<MockOcsp::CertificatePair>> ocspResponderKnownCertificateCaPairs
            = {
                  {"http://ocsp-testref.tsl.telematik-test/ocsp",
                      {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                          {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}
            };
        ocspResponderKnownCertificateCaPairs.insert(
            additionalOcspResponderKnownCertificateCaPairs.begin(),
            additionalOcspResponderKnownCertificateCaPairs.end());

        auto tslManager = TslTestHelper::createTslManager<TslManager>(
            {},
            {},
            ocspResponderKnownCertificateCaPairs);

        TslTestHelper::addCaCertificateToTrustStore(
            idpCertificateCa,
            *tslManager,
            TslMode::TSL);

        return tslManager;
    }

    void SetUp()
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    }

    void TearDown()
    {
        mCaDerPathGuard.reset();
    }
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
    IdpTestUpdater (Idp& certificateHolder, TslManager* tslManager)
        : IdpUpdater(certificateHolder, tslManager, {})
    {
    }

    size_t updateCount = 0;

protected:
    virtual std::string doDownloadWellknown (void) override
    {
        return "";
    }
    virtual UrlHelper::UrlParts doParseWellknown (std::string&&) override
    {
        ++updateCount;
        return {"","",443,"",""};
    }
    virtual std::string doDownloadDiscovery (const UrlHelper::UrlParts&) override
    {
        return "";
    }
    virtual std::vector<Certificate> doParseDiscovery (std::string&&) override
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
    IdpErrorUpdater (Idp& certificateHolder, TslManager* tslManager, const UpdateStatus mockStatus)
        : IdpUpdater(certificateHolder, tslManager, {}),
          mMockStatus(mockStatus)
    {
    }

    void setMockedUpdateStatus (const UpdateStatus mockStatus)
    {
        mMockStatus = mockStatus;
    }

    using UpdateStatus = IdpUpdater::UpdateStatus; // Make it accessible to tests.

protected:
    virtual std::string doDownloadWellknown (void) override
    {
        return "";
    }
    virtual UrlHelper::UrlParts doParseWellknown (std::string&&) override
    {
        if (mMockStatus == UpdateStatus::WellknownDownloadFailed)
            throw std::runtime_error("test");
        else
            return {"","",443,"",""};
    }
    virtual std::string doDownloadDiscovery (const UrlHelper::UrlParts&) override
    {
        return "";
    }
    virtual std::vector<Certificate> doParseDiscovery (std::string&&) override
    {
        if (mMockStatus == UpdateStatus::DiscoveryDownloadFailed)
            throw std::runtime_error("test");
        else
            return {StaticData::idpCertificate};
    }
    virtual void doVerifyCertificate (const std::vector<Certificate>&) override
    {
        if (mMockStatus == UpdateStatus::VerificationFailed)
            throw std::runtime_error("test");
    }
    virtual void reportUpdateStatus (UpdateStatus status, std::string_view details) override
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
    IdpBrokenCertificateUpdater(
        Idp& certificateHolder,
        TslManager* tslManager,
        const std::shared_ptr<UrlRequestSender>& urlRequestSender)
        : IdpUpdater(certificateHolder, tslManager, urlRequestSender)
    {
    }

    virtual std::vector<Certificate> doParseDiscovery (std::string&& jwtString) override
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
TEST_F(IdpUpdaterTest, update)
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
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    auto updater = IdpUpdater::create(
        idp,
        tslManager.get(),
        true,
        idpRequestSender);

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * Simulate response with broken IDP certificate.
 */
TEST_F(IdpUpdaterTest, updateWithBrokenResponse)
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
        tslManager.get(),
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
TEST_F(IdpUpdaterTest, DISABLED_update_resetForMaxAge)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Simulate an earlier successful update.
    idp.setCertificate(Certificate(StaticData::idpCertificate));
    ASSERT_NO_THROW(idp.getCertificate());

    // A missing successful update, i.e. directly after the application starts, is treated like it was
    // on the start of the epoch. That means it is more than 24 hours in the past and as a result the IDP
    // certificate is reset.
    IdpErrorUpdater updater (idp, tslManager.get(), IdpErrorUpdater::UpdateStatus::WellknownDownloadFailed);
    updater.update();

    // The failed update is expected to trigger the 24 hours maximum age of the certificate.
    ASSERT_ANY_THROW(idp.getCertificate());
}


/**
 * Simulate a failed update attempt. But this time the last successful attempt is not 24 hours in the past.
 */
TEST_F(IdpUpdaterTest, update_noResetForMaxAge)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();

    // Simulate an earlier successful update.
    idp.setCertificate(Certificate(StaticData::idpCertificate));
    ASSERT_NO_THROW(idp.getCertificate());

    // One successful update to set the lastSuccessfulUpdateTime to "now".
    IdpErrorUpdater updater (idp, tslManager.get(), IdpErrorUpdater::UpdateStatus::Success);
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
TEST_F(IdpUpdaterTest, updateStaticCertificate)
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
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    auto updater = IdpUpdater::create(
        idp,
        tslManager.get(),
        true,
        idpRequestSender);

    updater->update();

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * This set of static data comes from https://dth01.ibmgcloud.net/jira/browse/ERP-5948
 */
TEST_F(IdpUpdaterTest, updateStaticCertificate2)
{
    Idp idp;
    auto idpCertificateErp5948 = Certificate::fromPemString(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/erp-5948-certificate.pem"));
    auto idpCertificateGemKompCa10 = Certificate::fromPemString(
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
        tslManager.get(),
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
    IdpErrorUpdater(idp, tslManager.get(), IdpErrorUpdater::UpdateStatus::WellknownDownloadFailed)
        .update();
}


TEST_F(IdpUpdaterTest, update_failForDiscoveryDownloadFailed)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();
    IdpErrorUpdater(idp, tslManager.get(), IdpErrorUpdater::UpdateStatus::DiscoveryDownloadFailed)
        .update();
}


// Disabled while failed verifications are handled as warnings.
TEST_F(IdpUpdaterTest, DISABLED_update_failForVerificationFailed)
{
    Idp idp;
    auto tslManager = createAndSetupTslManager();
    IdpErrorUpdater(idp, tslManager.get(), IdpErrorUpdater::UpdateStatus::VerificationFailed)
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

    IdpTestUpdater updater (idp, tslManager.get());

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
TEST_F(IdpUpdaterTest, initializeWithForcedRetries)
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
                                    [&idpResponseJwk](const std::string&) -> ClientResponse
                                    {
                                        static int count=0;
                                        Header header;
                                        if (count++ < 2 )
                                        {
                                            header.setStatus(HttpStatus::NetworkConnectTimeoutError);
                                            header.setContentLength(0);
                                            return ClientResponse(header, "");
                                        }
                                        else
                                        {
                                            header.setStatus(HttpStatus::OK);
                                            header.setContentLength(idpResponseJwk.size());
                                            return ClientResponse(header, idpResponseJwk);
                                        }
                                    });

    auto updater = IdpUpdater::create(
        idp,
        tslManager.get(),
        true,
        idpRequestSender);

    // ... the certificate is set.
    ASSERT_NO_THROW(idp.getCertificate());
}


/**
 * Simulate an unstable IDP endpoint that never returns a valid response
 */
TEST_F(IdpUpdaterTest, initializeFailedWithForcedRetries)
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
                                      return ClientResponse(header, "");
                                    });

    auto updater = IdpUpdater::create(
        idp,
        tslManager.get(),
        true,
        idpRequestSender);

    // ... still no certificate is set due to failing download.
    ASSERT_ANY_THROW(idp.getCertificate());
}