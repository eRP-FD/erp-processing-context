#include "erp/pc/CFdSigErpManager.hxx"

#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"
#include "erp/hsm/HsmPool.hxx"


namespace
{
    // the default C.FD.OSIG eRP Signer certificate OCSP grace period value according to A_20765-02
    constexpr int defaultCFdSigErpOcspGracePeriodSeconds = 24 * 60 * 60;

// GEMREQ-start A_20974-01#intervall
    std::chrono::system_clock::duration getOcspRequestInterval(const Configuration& configuration)
    {
        static constexpr int defaultCFdSigErpValidationIntervalSeconds = 3600;
        std::chrono::seconds updateSeconds(
                configuration.getOptionalIntValue(ConfigurationKey::C_FD_SIG_ERP_VALIDATION_INTERVAL,
                                              defaultCFdSigErpValidationIntervalSeconds));
        return updateSeconds;
    }
// GEMREQ-end A_20974-01#intervall
}


// GEMREQ-start A_20974-01#creation,A_20765-02#creation
CFdSigErpManager::CFdSigErpManager(
    const Configuration& configuration,
    TslManager& tslManager,
    HsmPool& hsmPool)
    : TimerJobBase("CFdSigErpManager job", getOcspRequestInterval(configuration))
    , mMutex()
    , mTslManager(tslManager)
    , mValidationHookId()
    , mHsmPool(hsmPool)
    , mValidationTimer()
    , mOcspRequestGracePeriod(std::chrono::seconds(
          configuration.getOptionalIntValue(ConfigurationKey::OCSP_C_FD_SIG_ERP_GRACE_PERIOD,
                                            defaultCFdSigErpOcspGracePeriodSeconds)))
    , mLastProducedAt()
    , mLastOcspSuccess()
    , mLastOcspRequestSuccessful(false)
    , mCFdSigErpKey()
    , mPkBlob()
{
    // trigger the first execution synchronously
    executeJob();

    // If TSL is updated the validation has to be done again
    mValidationHookId = mTslManager.addPostUpdateHook([this] {
        getOcspResponseData(true);
    });

    start();
}
// GEMREQ-end A_20974-01#creation,A_20765-02#creation


CFdSigErpManager::~CFdSigErpManager()
{
    if (mValidationHookId.has_value())
    {
        mTslManager.disablePostUpdateHook(*mValidationHookId);
    }

    shutdown();
}


[[nodiscard]] Certificate CFdSigErpManager::getCertificate()
{
    auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();

    std::lock_guard lock(mMutex);

    // triggers certificate validation if necessary
    internalGetOcspResponseData(cFdSigErp, false);

    return cFdSigErp;
}


shared_EVP_PKEY CFdSigErpManager::getPrivateKey()
{
    std::lock_guard lock(mMutexPkCache);
    std::tie(mCFdSigErpKey, mPkBlob) = mHsmPool.acquire().session().getVauSigPrivateKey(mCFdSigErpKey, mPkBlob);
    return mCFdSigErpKey;
}


TrustStore::OcspResponseData CFdSigErpManager::getOcspResponseData(const bool forceOcspRequest)
{
    auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    std::lock_guard lock(mMutex);
    return internalGetOcspResponseData(cFdSigErp, forceOcspRequest);
}



OcspResponsePtr CFdSigErpManager::getOcspResponse()
{
    auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    std::lock_guard lock(mMutex);

    const std::optional<TrustStore::OcspResponseData> responseData = internalGetOcspResponseData(cFdSigErp, false);
    if (responseData.has_value())
    {
        return OcspHelper::stringToOcspResponse(responseData->response);
    }

    return {};
}


// GEMREQ-start A_20765-02#validation
TrustStore::OcspResponseData
CFdSigErpManager::internalGetOcspResponseData(const Certificate& certificate, const bool forceOcspRequest)
{
    // internal implementation call, it has to be guarded by mutex in caller public/protected methods
    mLastProducedAt = std::chrono::system_clock::time_point{};

    try
    {
        // after the Certificate and X509Certificate classes refactored to one class this workaround will no more be necessary,
        // it could be possible to share X509 pointer between classes, but this approach looks to be more safe
        auto x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

        const auto startPoint = std::chrono::system_clock::now();

        // get the OCSP response data, in case the retrieval is triggered by validation scenario
        // force validation of the certificate and new OCSP request, but still allow OCSP response cache fallback
        auto responseData =
            mTslManager.getCertificateOcspResponse(
                TslMode::TSL,
                x509Certificate,
                {CertificateType::C_FD_SIG, CertificateType::C_FD_OSIG},
                {
                    forceOcspRequest
                        ? OcspCheckDescriptor::OcspCheckMode::FORCE_OCSP_REQUEST_ALLOW_CACHE
                        : OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,
                    {std::nullopt, mOcspRequestGracePeriod},
                    {}});

        Expect(responseData.status.certificateStatus == OcspService::CertificateStatus::good,
               "TslManager::getCertificateOcspResponse() must only return in case of good ocsp status.");

        mLastProducedAt = responseData.producedAt;

        if (! responseData.fromCache || forceOcspRequest)
        {
            mLastOcspSuccess = responseData.receivedAt;
            // last OCSP request is only successful if we got a new result within this request
            mLastOcspRequestSuccessful = responseData.receivedAt >= startPoint;
            if (! mLastOcspRequestSuccessful)
            {
                TLOG(WARNING) << "OCSP request has failed, last successful OCSP response was done at "
                              << model::Timestamp(mLastOcspSuccess).toXsDateTime();
            }
        }

        return responseData;
    }
    catch(const TslError& tslError)
    {
        // in this context TslError always means service unavailable error
        throw ExceptionWrapper<TslError>::create({__FILE__, __LINE__},tslError, HttpStatus::ServiceUnavailable);
    }
}
// GEMREQ-end A_20765-02#validation


void CFdSigErpManager::healthCheck()
{
    std::lock_guard lock(mMutex);

    if (mLastProducedAt == std::chrono::system_clock::time_point{})
    {
        Fail2("no successful validation available", std::runtime_error);
    }

    const auto now = std::chrono::system_clock::now();
    if (mLastProducedAt + mOcspRequestGracePeriod < now)
    {
        Fail2("last validation is too old", std::runtime_error);
    }
}


std::string CFdSigErpManager::getLastOcspResponseTimestamp()
{
    std::lock_guard lock(mMutex);

    if (mLastProducedAt == std::chrono::system_clock::time_point{})
    {
        return "never successfully validated";
    }

    return model::Timestamp(mLastProducedAt).toXsDateTime();
}


bool CFdSigErpManager::wasLastOcspRequestSuccessful()
{
    std::lock_guard lock(mMutex);
    return mLastOcspRequestSuccessful;
}


CertificateType CFdSigErpManager::getCertificateType() const
{
    const auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    const auto x509Certificate = X509Certificate::createFromBase64(cFdSigErp.toBase64Der());
    return TslService::getCertificateType(x509Certificate);
}


std::string CFdSigErpManager::getCertificateNotAfterTimestamp() const
{
    const auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    const auto x509Certificate = X509Certificate::createFromBase64(cFdSigErp.toBase64Der());
    return model::Timestamp::fromTmInUtc(x509Certificate.getNotAfter()).toXsDateTime();
}


void CFdSigErpManager::onStart()
{
}


// GEMREQ-start A_20974-01#timerJob
void CFdSigErpManager::executeJob()
{
    try
    {
        TLOG(INFO) << "Running periodic FD.OSIG certificate validation";
        auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
        std::lock_guard lock(mMutex);

        internalGetOcspResponseData(cFdSigErp, true);
    }
    catch (const ::std::exception& e)
    {
        TLOG(ERROR) << "Error during planned validation of C.FD.SIG eRp Certificate: " << e.what();
    }
    catch (...)
    {
        TLOG(ERROR) << "Unexpected error during planned validation of C.FD.SIG eRp Certificate";
    }
}
// GEMREQ-end A_20974-01#timerJob


void CFdSigErpManager::onFinish()
{
}
