/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/CFdSigErpManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Configuration.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestUtils.hxx"

#include "test_config.h"

#include <gtest/gtest.h>
#include <mutex>
#include <unordered_map>


class CountingUrlRequestSenderMock : public UrlRequestSenderMock
{
public:
    explicit CountingUrlRequestSenderMock(std::unordered_map<std::string, std::string> responses)
        : UrlRequestSenderMock(responses)
        , mMutex()
        , mCounterMap()
    {
    }

    size_t getCounterMapSize()
    {
        std::lock_guard lockGuard(mMutex);
        return mCounterMap.size();
    }

    size_t getCounter(const std::string& url)
    {
        std::lock_guard lockGuard(mMutex);
        const auto& iterator = mCounterMap.find(url);
        if (iterator == mCounterMap.end())
        {
            return 0;
        }

        return iterator->second;
    }

protected:
    virtual ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const override
    {
        increaseCounter(url);
        return UrlRequestSenderMock::doSend(url, method, body, contentType, forcedCiphers, trustCn);
    }


    virtual ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false) const override
    {
        increaseCounter(url.toString());
        return UrlRequestSenderMock::doSend(url, method, body, contentType, forcedCiphers, trustCn);
    }

private:
    void increaseCounter(const std::string& url) const
    {
        std::lock_guard lockGuard(mMutex);
        ++mCounterMap[url];
    }

    mutable std::mutex mMutex;
    mutable std::unordered_map<std::string, size_t> mCounterMap;
};


class CFdSigErpManagerTest : public ::testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;
    std::unique_ptr<EnvironmentVariableGuard> mCFdSigErpGuard;


    void SetUp()
    {
        // a valid value for ConfigurationKey::C_FD_SIG_ERP must be set in the testing environment already
        // but to be sure it is set here explicitly
        mCFdSigErpGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_C_FD_SIG_ERP",
            CFdSigErpTestHelper::cFdSigErp);
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    }


    void TearDown()
    {
        mCaDerPathGuard.reset();
    }
};


TEST_F(CFdSigErpManagerTest, noTslManager)
{
   CFdSigErpManager cFdSigErpManager(Configuration::instance(), nullptr);

   EXPECT_FALSE(cFdSigErpManager.getOcspResponseData(true).has_value());
   EXPECT_FALSE(cFdSigErpManager.getOcspResponseData(false).has_value());
   EXPECT_EQ(cFdSigErpManager.getOcspResponse(), nullptr);
   EXPECT_EQ(cFdSigErpManager.getLastValidationTimestamp(), "never successfully validated");
   EXPECT_THROW(cFdSigErpManager.healthCheck(), std::runtime_error);

   EXPECT_TRUE(cFdSigErpManager.getCertificate().toX509().isSet());
}


TEST_F(CFdSigErpManagerTest, tslManagerSet_NoOcspConnection_fail)
{
    // default mocking does not support C.FD.SIG eRP Certificate OCSP request
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>();

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), tslManager);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getCertificate(),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::InternalServerError);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(true),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::InternalServerError);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(false),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::InternalServerError);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponse(),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::InternalServerError);
    EXPECT_EQ(cFdSigErpManager.getLastValidationTimestamp(), "never successfully validated");
    EXPECT_THROW(cFdSigErpManager.healthCheck(), std::runtime_error);
}


TEST_F(CFdSigErpManagerTest, tslManagerSet_success)
{
    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPemString(CFdSigErpTestHelper::cFdSigErp);
    auto certCA = Certificate::fromPemString(CFdSigErpTestHelper::cFdSigErpSigner);
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl);
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), tslManager);
    const auto responseData = cFdSigErpManager.getOcspResponseData(false);
    ASSERT_TRUE(responseData.has_value());
    EXPECT_NE(cFdSigErpManager.getOcspResponse(), nullptr);
    EXPECT_EQ(cFdSigErpManager.getLastValidationTimestamp(), model::Timestamp(responseData->timeStamp).toXsDateTime());
    EXPECT_NO_THROW(cFdSigErpManager.healthCheck());
    EXPECT_TRUE(cFdSigErpManager.getCertificate().toX509().isSet());

    // 2 URLs for TSL + 2 URLs for BNA + 1 URL for TSL Signer OCSP-Request + 1 URL for C.FD.SIG eRP OCSP-Request
    ASSERT_EQ(requestSender->getCounterMapSize(), 6);
    // after the first OCSP-Request the following calls to CFdSigErpManager must use the cached OCSP-Response
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // the call should force OCSP-request
    EXPECT_TRUE(cFdSigErpManager.getOcspResponseData(true).has_value());
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 2);
}


TEST_F(CFdSigErpManagerTest, timerUpdate_success)
{
    EnvironmentVariableGuard ocspGracePeriodGuard("ERP_OCSP_NON_QES_GRACE_PERIOD", "1");

    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPemString(CFdSigErpTestHelper::cFdSigErp);
    auto certCA = Certificate::fromPemString(CFdSigErpTestHelper::cFdSigErpSigner);
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl);
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), tslManager);

    // 2 URLs for TSL + 2 URLs for BNA + 1 URL for TSL Signer OCSP-Request + 1 URL for C.FD.SIG eRP OCSP-Request
    ASSERT_EQ(requestSender->getCounterMapSize(), 6);
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // wait two seconds, the timer must do the validation again during this time
    waitFor([&requestSender, &ocspUrl] () -> bool {return requestSender->getCounter(ocspUrl) > 1;});
}