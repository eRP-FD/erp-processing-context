/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tsl/TrustStore.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/TslParser.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"


namespace
{
    bool isServiceAcceptable(const TslParser::AcceptanceHistoryMap& serviceAcceptanceHistory,
                             const std::optional<X509Certificate>& certificate)
    {
        Expect( !serviceAcceptanceHistory.empty(),
                "service acceptance history must not be empty at this point");
        Expect(!certificate.has_value() || certificate->checkValidityPeriod(),
               "the certificate must be valid");

        if (serviceAcceptanceHistory.size() == 1 && serviceAcceptanceHistory.begin()->second)
        {
            // if there is no history and the Service is accepted ignore the StatusStartingTime of the service,
            // it affects all TSL Types: Gematik-TSL and BNetzA-VL
            return true;
        }

        // look for the latest entry in the map that is out of certificate validity period,
        // this entry represents the CA-ServiceStatus at the point of certificate validity start
        for (auto iterator = serviceAcceptanceHistory.rbegin(); //NOLINT(modernize-loop-convert)
             iterator != serviceAcceptanceHistory.rend();
             ++iterator)
        {
            if (!certificate.has_value() || !certificate->checkValidityPeriod(iterator->first))
            {
                return iterator->second;
            }
        }

        return false;
    }


    bool checkNewTslSignerCaIdList(
        const TslParser& tslParser,
        TslParser::ServiceInformationMap& serviceInformationMap)
    {
        bool result = true;
        if (tslParser.getNewTslSignerCaIdList().size() == 1)
        {
            const CertificateId& id = tslParser.getNewTslSignerCaIdList()[0];
            const auto iterator = serviceInformationMap.find(id);
            Expect(iterator != serviceInformationMap.end(), "The entry for ID must exist");
            Expect(!iterator->second.serviceAcceptanceHistory.empty(), "StatusStartingTime must be set");

            if (isServiceAcceptable(iterator->second.serviceAcceptanceHistory, std::nullopt))
            {
                // show the warning only for the valid certificate
                const model::Timestamp timestamp(iterator->second.serviceAcceptanceHistory.begin()->first);
                TLOG(WARNING) << TslError("Important: new TSL CA is going to be active from " + timestamp.toXsDateTime()
                                         + ", [" + iterator->second.certificate.toBase64() + "]",
                                         TslErrorCode::TSL_CA_UPDATE_WARNING).what();
            }
            else
            {
                // the certificates are rejected and can not be used as new CA
                // Gematik does not require to output an error in this case
                TLOG(WARNING) << "New TSL CA is provided in TSL, but it is revoked.";
                result = false;
            }
        }
        else if (tslParser.getNewTslSignerCaIdList().size() > 1)
        {
            // According to TUC_PKI_013 the problem with multiple CAs must be logged,
            // but it should not affect other CAs
            TLOG(ERROR) << TslError("Only one new TI-Trust-Anchor is allowed",
                                   TslErrorCode::MULTIPLE_TRUST_ANCHOR).what();

            result = false;
        }

        return result;
    }


    X509Certificate getCertificateFromDerFile(const std::string& path)
    {
        // In this case the file contains binary data, not base64 encoded.
        const auto rawTslSignerCaCert = FileHelper::readFileAsString(path);
        return X509Certificate::createFromAsnBytes(
            { reinterpret_cast<const unsigned char*>(rawTslSignerCaCert.data()), rawTslSignerCaCert.size() });
    }


    X509Certificate getMainTslSignerCa()
    {
        TVLOG(2) << "getTslSignerCa";

        const std::string initialCaDerPath = Configuration::instance().getOptionalStringValue(
            ConfigurationKey::TSL_INITIAL_CA_DER_PATH, "/erp/config/tsl/tsl-ca.der");

        TVLOG(2) << "initial TSL Signer CA path: " << initialCaDerPath;
        Expect(!initialCaDerPath.empty(), "Initial TSL signer CA path must be set.");
        Expect(FileHelper::exists(initialCaDerPath), "Initial TSL signer CA file must exist");

        return getCertificateFromDerFile(initialCaDerPath);
    }


    std::optional<X509Certificate> getNewTslSignerCa()
    {
        TVLOG(2) << "getNewTslSignerCa";
        const std::string initialCaDerPathNew = Configuration::instance().getOptionalStringValue(
            ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW, "");
        if (!initialCaDerPathNew.empty())
        {
            TVLOG(0) << "New TSL Signer CA is configured.";
            return getCertificateFromDerFile(initialCaDerPathNew);
        }

        return std::nullopt;
    }
}


TrustStore::TrustStore (const TslMode mode, std::vector<std::string> initialTslUrls)
    : mMutex{}
    , mMode(mode)
    , mTslStored{false}
    , mTslHashValue{}
    , mId{}
    , mSequenceNumber{}
    , mNextUpdate{}
    , mLastUpdateCheck{}
    , mUpdateUrls(std::move(initialTslUrls))
    , mBnaServiceInformation{}
    , mServiceInformationMap{}
    , mOcspCache{}
{
}


TslMode TrustStore::getTslMode() const
{
    return mMode;
}


std::vector<X509Certificate> TrustStore::getTslSignerCas() const
{
    const X509Certificate mainTslSignerCa = getMainTslSignerCa();
    const std::optional<X509Certificate> newTslSignerCa = getNewTslSignerCa();
    const std::string newTslSignerCaStart = Configuration::instance().getOptionalStringValue(
        ConfigurationKey::TSL_INITIAL_CA_DER_PATH_NEW_START, "");

    std::vector<X509Certificate> tslSignerCaCerts{
        mainTslSignerCa
    };

    if (newTslSignerCa.has_value())
    {
        TVLOG(0) << "start: " << newTslSignerCaStart;
        if (newTslSignerCaStart.empty()
            || model::Timestamp::now() > model::Timestamp::fromFhirDateTime(newTslSignerCaStart))
        {
            TVLOG(0) << "New TSL Signer CA is used.";
            tslSignerCaCerts.emplace_back(*newTslSignerCa);
        }
        else
        {
            TVLOG(0) << "New TSL Signer CA is configured for future and is ignored.";
        }
    }

    return tslSignerCaCerts;
}


std::optional<std::string> TrustStore::getTslHashValue() const
{
    std::lock_guard guard(mMutex);
    return mTslHashValue;
}


bool TrustStore::hasTsl() const
{
    std::lock_guard guard(mMutex);
    return mTslStored;
}


std::optional<TrustStore::CaInfo> TrustStore::lookupCaCertificate (
    const X509Certificate& certificate) const
{
    std::lock_guard guard(mMutex);

    auto it = mServiceInformationMap.find({certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    if (it != mServiceInformationMap.end()
        && certificate.getAuthorityKeyIdentifier() == it->second.certificate.getSubjectKeyIdentifier())
    {
        return TrustStore::CaInfo{it->second.certificate,
                                  isServiceAcceptable(it->second.serviceAcceptanceHistory, certificate),
                                  it->second.extensionOidList};
    }

    return std::nullopt;
}


bool TrustStore::hasCaCertificateWithSubject(const std::string& subjectDn) const
{
    std::lock_guard guard(mMutex);

    // this implementation is only used in error-handling scenarios,
    // and thus O(n) complexity looks to be OK
    for (const auto& [id, data] : mServiceInformationMap) // NOLINT(readability-use-anyofallof)
    {
        (void)data;
        if (id.subject == subjectDn)
            return true;
    }

    return false;
}


X509Store TrustStore::getX509Store(const std::optional<X509Certificate>& certificate) const
{
    std::lock_guard guard(mMutex);

    if (mServiceInformationMap.empty())
    {
        return X509Store();
    }

    std::vector<X509Certificate> trustedCertificates;
    for (const auto& [subject, serviceInformation] : mServiceInformationMap)
    {
        (void)subject;
        if (isServiceAcceptable(serviceInformation.serviceAcceptanceHistory, certificate))
        {
            trustedCertificates.emplace_back(serviceInformation.certificate);
        }
    }
    return X509Store(std::move(trustedCertificates));
}


// gemSpec_Frontend_Vers § 6.1.5.2 ePA-Frontend des Versicherten: TSL - Truststore für
// Zertifikatsprüfung
// GEMREQ-start A_17732
void TrustStore::refillFromTsl(TslParser& tslParser)
{
    std::lock_guard guard(mMutex);

    mId = tslParser.getId();
    mSequenceNumber = tslParser.getSequenceNumber();
    mNextUpdate = tslParser.getNextUpdate();
    mUpdateUrls = tslParser.getUpdateUrls();
    mBnaServiceInformation = tslParser.getBnaServiceInformation();
    mServiceInformationMap = tslParser.getServiceInformationMap();

    if (mMode == TslMode::TSL && !checkNewTslSignerCaIdList(tslParser, mServiceInformationMap))
    {
        // the problematic CAs related to TUC_PKI_013 should be removed
        for (const CertificateId& id : tslParser.getNewTslSignerCaIdList())
        {
            mServiceInformationMap.erase(id);
        }
    }

    mTslStored = true;
}
// GEMREQ-end A_17732


void TrustStore::setTslHashValue (const std::optional<std::string>& tslHashValue)
{
    std::lock_guard guard(mMutex);
    mTslHashValue = tslHashValue;
}


std::string TrustStore::getIdOfTslInUse() const
{
    std::lock_guard guard(mMutex);
    return mId;
}


std::string TrustStore::getSequenceNumberOfTslInUse() const
{
    std::lock_guard guard(mMutex);
    return mSequenceNumber;
}


std::chrono::system_clock::time_point TrustStore::getNextUpdate() const
{
    std::lock_guard guard(mMutex);
    return mNextUpdate;
}


std::vector<std::string> TrustStore::getUpdateUrls() const
{
    std::lock_guard guard(mMutex);
    return mUpdateUrls;
}


void TrustStore::setUpdateUrls(const std::vector<std::string>& updateUrls)
{
    std::lock_guard guard(mMutex);
    mUpdateUrls = updateUrls;
}


std::vector<std::string> TrustStore::getBnaUrls() const
{
    std::lock_guard guard(mMutex);
    return mBnaServiceInformation.supplyPointList;
}


std::vector<X509Certificate> TrustStore::getBnaSignerCertificates() const
{
    std::lock_guard guard(mMutex);
    return mBnaServiceInformation.signerCertificateList;
}


std::unordered_map<std::string, std::string> TrustStore::getBnaOcspMapping() const
{
    std::lock_guard guard(mMutex);
    return mBnaServiceInformation.ocspMapping;
}


void TrustStore::setBnaOcspMapping(std::unordered_map<std::string, std::string> ocspMapping)
{
    std::lock_guard guard(mMutex);
    mBnaServiceInformation.ocspMapping = std::move(ocspMapping);
}


void TrustStore::distrustCertificates ()
{
    std::lock_guard guard(mMutex);

    mServiceInformationMap.clear();
    mBnaServiceInformation = {};
}


bool TrustStore::isTslTooOld () const
{
    std::lock_guard guard(mMutex);
    return getNextUpdate() <= std::chrono::system_clock::now();
}


void TrustStore::setCacheOcspData (const std::string& fingerprint, OcspResponseData ocspCacheData)
{
    std::lock_guard guard(mMutex);
    mOcspCache[fingerprint] = std::move(ocspCacheData);
}


void TrustStore::cleanCachedOcspData(const std::string& fingerprint)
{
     std::lock_guard guard(mMutex);

    cleanupOcspCache();
    mOcspCache.erase(fingerprint);
}


std::optional<TrustStore::OcspResponseData> TrustStore::getCachedOcspData (const std::string& fingerprint)
{
    std::lock_guard guard(mMutex);

    cleanupOcspCache();

    auto it = mOcspCache.find(fingerprint);
    if (it != mOcspCache.end())
    {
        return it->second;
    }

    return {};
}


void TrustStore::cleanupOcspCache ()
{
    std::lock_guard guard(mMutex);

    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();

    for (auto it = mOcspCache.begin(), ite = mOcspCache.end(); it != ite;)
    {
        const bool olderThanGracePeriod = ((now - it->second.producedAt) > it->second.gracePeriod);
        if (olderThanGracePeriod)
            it = mOcspCache.erase(it);
        else
            ++it;
    }
}


bool TrustStore::isOcspResponderInTsl (
    X509* ocspResponder) const
{
    X509Certificate x509Certificate = X509Certificate::createFromX509Pointer(ocspResponder);
    return isCertificateInTsl(x509Certificate);
}


bool TrustStore::isCertificateInTsl (
    const X509Certificate& certificate) const
{
    std::lock_guard guard(mMutex);

    const auto it = mServiceInformationMap.find({certificate.getSubject(), certificate.getSubjectKeyIdentifier()});
    return it != mServiceInformationMap.end() && it->second.certificate == certificate;
}


bool TrustStore::certificateHasTypeIdentifier (
    const X509Certificate& certificate,
    const std::string& typeIdentifier) const
{
    std::lock_guard guard(mMutex);

    const auto it = mServiceInformationMap.find({certificate.getSubject(), certificate.getSubjectKeyIdentifier()});
    return (it != mServiceInformationMap.end() && it->second.serviceIdentifier == typeIdentifier);
}


std::optional<std::string> TrustStore::primaryOcspServiceUrlForCertificate(const X509Certificate& certificate)
{
    std::lock_guard guard(mMutex);

    const auto itr = mServiceInformationMap.find({certificate.getSubject(), certificate.getSubjectKeyIdentifier()});
    if (mServiceInformationMap.cend() != itr && !itr->second.serviceSupplyPointList.empty())
    {
        return itr->second.serviceSupplyPointList.front();
    }

    return std::nullopt;
}
