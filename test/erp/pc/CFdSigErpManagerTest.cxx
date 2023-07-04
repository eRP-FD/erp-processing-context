/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/CFdSigErpManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Configuration.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/LogTestBase.hxx"
#include "test/util/TestUtils.hxx"

#include "test_config.h"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <mutex>
#include <unordered_map>


class CountingUrlRequestSenderMock : public UrlRequestSenderMock
{
public:
    explicit CountingUrlRequestSenderMock(std::unordered_map<std::string, std::string> responses)
        : UrlRequestSenderMock(std::move(responses))
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
    ClientResponse doSend (
        const std::string& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* /*ep*/ = nullptr) const override
    {
        increaseCounter(url);
        return UrlRequestSenderMock::doSend(url, method, body, contentType, forcedCiphers, trustCn);
    }


    ClientResponse doSend (
        const UrlHelper::UrlParts& url,
        const HttpMethod method,
        const std::string& body,
        const std::string& contentType = std::string(),
        const std::optional<std::string>& forcedCiphers = std::nullopt,
        const bool trustCn = false,
        const boost::asio::ip::tcp::endpoint* /*ep*/ = nullptr) const override
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


    void SetUp() override
    {
        // a valid value for ConfigurationKey::C_FD_SIG_ERP must be set in the testing environment already
        // but to be sure it is set here explicitly
        mCFdSigErpGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_C_FD_SIG_ERP",
            CFdSigErpTestHelper::cFdSigErp());
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
    }


    void TearDown() override
    {
        mCaDerPathGuard.reset();
    }

    PcServiceContext mContext{StaticData::makePcServiceContext()};
};


TEST_F(CFdSigErpManagerTest, tslManagerSet_NoOcspConnection_fail)//NOLINT(readability-function-cognitive-complexity)
{
    // default mocking does not support C.FD.SIG eRP Certificate OCSP request
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>();

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());
    EXPECT_TSL_ERROR_THROW(
        (void)cFdSigErpManager.getCertificate(),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::ServiceUnavailable);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(true),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::ServiceUnavailable);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(false),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::ServiceUnavailable);
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponse(),
        {TslErrorCode::OCSP_NOT_AVAILABLE},
        HttpStatus::ServiceUnavailable);
    EXPECT_EQ(cFdSigErpManager.getLastOcspResponseTimestamp(), "never successfully validated");
    EXPECT_THROW(cFdSigErpManager.healthCheck(), std::runtime_error);
}


TEST_F(CFdSigErpManagerTest, tslManagerSet_success)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());
    const auto responseData = cFdSigErpManager.getOcspResponseData(false);
    EXPECT_NE(cFdSigErpManager.getOcspResponse(), nullptr);
    EXPECT_EQ(cFdSigErpManager.getLastOcspResponseTimestamp(), model::Timestamp(responseData.producedAt).toXsDateTime());
    EXPECT_NO_THROW(cFdSigErpManager.healthCheck());
    EXPECT_TRUE(cFdSigErpManager.getCertificate().toX509().isSet());

    // 2 URLs for TSL + 2 URLs for BNA + 1 URL for TSL Signer OCSP-Request + 1 URL for C.FD.SIG eRP OCSP-Request
    ASSERT_EQ(requestSender->getCounterMapSize(), 6);
    // after the first OCSP-Request the following calls to CFdSigErpManager must use the cached OCSP-Response
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // the call should force OCSP-request
    EXPECT_NO_FATAL_FAILURE(cFdSigErpManager.getOcspResponseData(true));
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 2);
}


TEST_F(CFdSigErpManagerTest, timerUpdate_success)
{
    EnvironmentVariableGuard ocspGracePeriodGuard("ERP_C_FD_SIG_ERP_VALIDATION_INTERVAL", "1");

    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());

    // 2 URLs for TSL + 2 URLs for BNA + 1 URL for TSL Signer OCSP-Request + 1 URL for C.FD.SIG eRP OCSP-Request
    ASSERT_EQ(requestSender->getCounterMapSize(), 6);
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // wait two seconds, the timer must do the validation again during this time
    testutils::waitFor([&requestSender, &ocspUrl] () -> bool {return requestSender->getCounter(ocspUrl) > 1;});
}


TEST_F(CFdSigErpManagerTest, timerUpdate_OCSP_fails_validation_success)
{
    EnvironmentVariableGuard ocspGracePeriodGuard("ERP_C_FD_SIG_ERP_VALIDATION_INTERVAL", "1");

    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());

    // 2 URLs for TSL + 2 URLs for BNA + 1 URL for TSL Signer OCSP-Request + 1 URL for C.FD.SIG eRP OCSP-Request
    ASSERT_EQ(requestSender->getCounterMapSize(), 6);
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    requestSender->setUrlHandler(ocspUrl,
                                 [](const std::string&) -> ClientResponse
                                 {
                                     Header header;
                                     header.setStatus(HttpStatus::NetworkConnectTimeoutError);
                                     header.setContentLength(0);
                                     return {header, ""};
                                 });

    // wait two seconds, the timer must do the validation again during this time
    testutils::waitFor([&requestSender, &ocspUrl] () -> bool {return requestSender->getCounter(ocspUrl) > 1;});

    // The second OCSP response request has failed, but that does not affect validity of C-FD-OSIG eRP certificate
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 2);
    ASSERT_FALSE(cFdSigErpManager.wasLastOcspRequestSuccessful());
    ASSERT_NO_THROW(cFdSigErpManager.getOcspResponseData(false));
    ASSERT_NO_THROW(cFdSigErpManager.healthCheck());
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 2);
}


TEST_F(CFdSigErpManagerTest, ocspStatusUnknown_fail)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    // let the OCSP-Response Status for C.FD.SIG certificate be set to unknown
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {{ocspUrl, {}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());

    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(false),
        {TslErrorCode::CERT_UNKNOWN},
        HttpStatus::ServiceUnavailable);

    // the second call is done to test handling of the OCSP-Response from cache
    EXPECT_TSL_ERROR_THROW(
        cFdSigErpManager.getOcspResponseData(false),
        {TslErrorCode::CERT_UNKNOWN},
        HttpStatus::ServiceUnavailable);
}


TEST_F(CFdSigErpManagerTest, signatureStatusValid_withinGracePeriod)
{
    LogTestBase::TestLogSink logSink;
    EnvironmentVariableGuard ocspGracePeriodGuard(ConfigurationKey::C_FD_SIG_ERP_VALIDATION_INTERVAL, "10");

    std::shared_ptr<CountingUrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<CountingUrlRequestSenderMock>();

    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        requestSender, {}, {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    CFdSigErpManager cFdSigErpManager(Configuration::instance(), *tslManager, mContext.getHsmPool());

    // get the ocsp once to fill the cache
    ASSERT_TRUE(cFdSigErpManager.wasLastOcspRequestSuccessful());
    // health check
    ASSERT_NO_THROW(cFdSigErpManager.healthCheck());
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // request a second time, no ocsp request expected, instead taken from cache
    EXPECT_NE(cFdSigErpManager.getOcspResponse(), nullptr);
    ASSERT_NO_THROW(cFdSigErpManager.healthCheck());
    ASSERT_TRUE(cFdSigErpManager.wasLastOcspRequestSuccessful());
    ASSERT_EQ(requestSender->getCounter(ocspUrl), 1);

    // force a new OCSP request and simulate a network error
    requestSender->setUrlHandler(ocspUrl, [](const std::string&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::NetworkConnectTimeoutError);
        header.setContentLength(0);
        return {header, ""};
    });

    cFdSigErpManager.getOcspResponseData(true);
    ASSERT_FALSE(cFdSigErpManager.wasLastOcspRequestSuccessful());

    // request the same data, but allow a cache hit
    EXPECT_NE(cFdSigErpManager.getOcspResponse(), nullptr);
    ASSERT_FALSE(cFdSigErpManager.wasLastOcspRequestSuccessful());

    // we expect only a single warning (from the first request)
    int warnings{0};
    logSink.visitLines([&](const std::string& line) {
        if (line.find("OCSP request has failed, last successful OCSP response was done at") != std::string::npos)
            warnings++;
    });
    ASSERT_EQ(warnings, 1);
}
