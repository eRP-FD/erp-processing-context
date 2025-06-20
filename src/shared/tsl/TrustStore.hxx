/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TSL_TRUSTSTORE_HXX
#define ERP_PROCESSING_CONTEXT_TSL_TRUSTSTORE_HXX

#include <boost/core/noncopyable.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "shared/tsl/OcspResponse.hxx"
#include "shared/tsl/TslMode.hxx"
#include "shared/tsl/TslParser.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/X509Store.hxx"
#include "shared/crypto/OpenSsl.hxx"


class TrustStore
    : private boost::noncopyable
{
public:

    class CaInfo
    {
    public:
        X509Certificate certificate;
        bool accepted;
        TslParser::ExtensionOidList extensionOidList;
    };

    struct HealthData {
        bool hasTsl{false};
        bool outdated{true};
        std::optional<std::string> hash;
        std::chrono::system_clock::time_point nextUpdate;
        std::optional<std::string> id;
        std::string sequenceNumber;
    };

    explicit TrustStore(const TslMode mode, std::vector<std::string> initialTslUrls = {});
    virtual ~TrustStore() = default;

    TslMode getTslMode() const;

    HealthData getHealthData() const;

    /**
      * Returns all signer certificates (trust anchors) that are currently accepted.
      * (This will typically return one certificate, but sometimes two due to an ongoing transition.)
      */
    virtual std::vector<X509Certificate> getTslSignerCas () const;

    /**
     * Refills the trust store from a TslParser.
     *
     * Will remove all stored CA certificates from the store.
     */
    void refillFromTsl(TslParser& tslParser);

    /**
     * Allows to find the CA for the certificate in trust store.
     */
    std::optional<CaInfo> lookupCaCertificate (
        const X509Certificate& certificate) const;

    /**
     * Return true if at least one certificate in the trust store has the given subject name.
     */
    bool hasCaCertificateWithSubject (const std::string& subjectDn) const;

    /**
     * Returns X509 trust store filled with TSL certificates for the specified point of time.
     * The TSL-Service history is used to create the store of trusted CAs for the specified certificate.
     * Depending from certificate starting validity date the set of trusted CAs could differ.
     *
     * If the trust store is not initialized the X509 trust store is just empty.
     */
    X509Store getX509Store(const std::optional<X509Certificate>& certificate) const;

    std::optional<std::string> getTslHashValue() const;

    void setTslHashValue (const std::optional<std::string>& tslHashValue);

    std::optional<std::string> getIdOfTslInUse() const;

    std::string getSequenceNumberOfTslInUse() const;

    std::chrono::system_clock::time_point getNextUpdate() const;

    /**
     * The update URLs can be set explicitly in case of TSL-mode BNA,
     * because they are provided in the main TSL.
     */
    void setUpdateUrls(const std::vector<std::string>& updateUrls);
    std::vector<std::string> getUpdateUrls() const;

    std::vector<std::string> getBnaUrls() const;

    std::vector<X509Certificate> getBnaSignerCertificates() const;

    std::unordered_map<std::string, std::string> getBnaOcspMapping() const;
    void setBnaOcspMapping(std::unordered_map<std::string, std::string> ocspMapping);

    bool hasTsl() const;

    bool isTslTooOld () const;

    bool isOcspResponderInTsl (X509* ocspResponder) const;

    /**
     * Checks if a certificate could be found identically in the TSL.
     * @param certificate to search for
     */
    bool isCertificateInTsl (const X509Certificate& certificate) const;

    /**
     * Checks if a certificate could be found under a given type identifier.
     *
     * @param certificate     the certificate to search for
     * @param typeIdentifier  the type identifier to look for
     */
    bool certificateHasTypeIdentifier (const X509Certificate& certificate,
                                       const std::string& typeIdentifier) const;

    void distrustCertificates ();

    /**
     * Caches ocsp response status with fingerprint, grace period, timestamp and response
     */
    void setCacheOcspData (const std::string& fingerprint, OcspResponse ocspCacheData);

    /**
     * Removes cached ocsp response data based on certificate fingerprint.
     */
    void cleanCachedOcspData (const std::string& fingerprint);

    /**
     * Returns cached OcspCacheData, if any.
     */
    std::optional<OcspResponse> getCachedOcspData (const std::string& fingerprint);


    /**
     * get OCSP service endpoint uri for a certificate CA.
     *
     * @return (optional) primary serviceEndpointUri (first if there is more than 1)
     */
    std::optional<std::string> primaryOcspServiceUrlForCertificate(const X509Certificate& certificate);

private:
    mutable std::recursive_mutex mMutex;

    const TslMode mMode;

    bool mTslStored{false};
    std::optional<std::string> mTslHashValue;
    std::optional<std::string> mId;
    std::string mSequenceNumber;
    std::chrono::system_clock::time_point mNextUpdate;
    std::chrono::system_clock::time_point mLastUpdateCheck;
    std::vector<std::string> mUpdateUrls;

    /// BNetzA-VL ServiceInformation from original TSL
    TslParser::BnaServiceInformation mBnaServiceInformation;

    /// {subjectDN, subjectKeyIdentifier} -> TSL service information
    TslParser::ServiceInformationMap mServiceInformationMap;

    /// fingerprint -> OcspCacheData
    std::map<std::string, OcspResponse> mOcspCache;

    /**
     * Removes cached OcspService results if they are older than 4 hours. (A_15873)
     */
    void cleanupOcspCache ();

    // Make private methods available to gtest.
#ifdef FRIEND_TEST

    FRIEND_TEST(TrustStoreTest, TrustStoreInitallyIsEmpty);
    FRIEND_TEST(TrustStoreTest, DISABLED_MultipleNewCATslXmlIsParsedCorrectly);
    FRIEND_TEST(TslServiceTest, providedTsl);
    FRIEND_TEST(TslServiceTest, verifyCertificateRevokedCAFailing);
    FRIEND_TEST(TslServiceTest, verifyCertificateValidThenRevokedCASuccess);
    FRIEND_TEST(TslServiceTest, verifyCertificatePolicyNoRestrictionsSuccessful);
    FRIEND_TEST(TslServiceTest, verifyCertificatePolicySuccessful);
    FRIEND_TEST(TslServiceTest, doNotCacheProvidedOcspResponse);

    friend class TslTestHelper;

#endif

#if WITH_HSM_MOCK > 0
    friend class MockTslManager;
#endif
};

#endif
