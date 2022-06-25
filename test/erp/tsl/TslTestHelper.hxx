/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef EPA_TEST_UTIL_TSLHELPER_HXX
#define EPA_TEST_UTIL_TSLHELPER_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <test_config.h>


#define EXPECT_TSL_ERROR_THROW(expression, tslErrorCodes, status) \
try \
{ \
    (expression); \
    FAIL() << "No exception is thrown."; \
} \
catch(const TslError& e) \
{ \
    const std::vector<TslErrorCode>& expectedCodes = tslErrorCodes; \
    EXPECT_EQ(status, e.getHttpStatus()); \
    EXPECT_EQ(expectedCodes.size(), e.getErrorData().size()); \
    for (size_t ind = 0; ind < expectedCodes.size(); ind++) \
    { \
        EXPECT_EQ(expectedCodes[ind], e.getErrorData()[ind].errorCode); \
    } \
}


class TslTestHelper
{
public:
    static constexpr const char* tslDownloadUrl = "http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml";
    static constexpr const char* shaDownloadUrl = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2";

    /**
     * Create TSL contents that are based on the given template but has its ##NEXT-UPDATE-DATE## placeholder
     * replaced with the current time plus 1 hour.
     */
    static std::string createValidButManipulatedTslContentsFromTemplate(const std::string& tslTemplate);

    static Certificate getDefaultOcspCertificate();
    static shared_EVP_PKEY getDefaultOcspPrivateKey();
    static Certificate getDefaultOcspCertificateCa();
    static Certificate getTslSignerCertificate();
    static Certificate getTslSignerCACertificate();

    static void setOcspUrlRequestHandler(
        UrlRequestSenderMock& requestSender,
        const std::string& url,
        const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs);
    static void setOcspUslRequestHandlerTslSigner(UrlRequestSenderMock& requestSender);

    static void addOcspCertificateToTrustStore(
        const Certificate& certificate,
        TrustStore& trustStore);

    static void addCaCertificateToTrustStore(
        const Certificate& certificate,
        TslManager& tslManager,
        const TslMode mode,
        const std::optional<std::string>& customSubjectKeyIdentifier = std::nullopt,
        const std::optional<std::string>& customCertificateTypeId = std::nullopt);

    static void setCaCertificateTimestamp(
        const std::chrono::time_point<std::chrono::system_clock> timestamp,
        const Certificate& certificate,
        TslManager& tslManager,
        const TslMode mode);

    static void removeCertificateFromTrustStore(
        const Certificate& certificate,
        TslManager& tslManager,
        const TslMode mode);

    static std::unique_ptr<TrustStore> createTslTrustStore (
        const std::optional<std::string>& tslContent = std::nullopt);

    static std::unique_ptr<TrustStore> createOutdatedTslTrustStore (
        const std::optional<std::string>& customTslDownloadUrl = std::nullopt);

    /**
     * Create a TslManager for testing, the returned pointer is never null.
     *
     * @tparam Manager                  TslManager class or a class derived from it
     *
     * @param customRequestSender       optional, allows to overwrite default test request sender
     * @param initialPostUpdateHooks    optional, allows to specify cusom hooksa
     * @param additionalUrlHandlers     optional, allows to provide additional certificates known for OCSP handlers
     * @param ocspCertificate           optional, allows to set custom expected OCSP certificate
     */
    template<class Manager>
    static std::shared_ptr<Manager> createTslManager (
        const std::shared_ptr<UrlRequestSenderMock>& customRequestSender = {},
        const std::vector<TslManager::PostUpdateHook>& initialPostUpdateHooks = {},
        const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& ocspResponderKnownCertificateCaPairs = {},
        const std::optional<Certificate>& ocspCertificate = std::nullopt,
        std::unique_ptr<TrustStore> tslTrustStore = {});
};


#endif
