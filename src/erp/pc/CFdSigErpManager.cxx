#include "erp/pc/CFdSigErpManager.hxx"

#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"
#include "erp/hsm/HsmPool.hxx"


namespace
{

// GEMREQ-start A_20974-01#intervall
    std::chrono::system_clock::duration getOcspRequestInterval(const Configuration& configuration)
    {
        std::chrono::seconds updateSeconds(
            configuration.getIntValue(ConfigurationKey::C_FD_SIG_ERP_VALIDATION_INTERVAL));
        return updateSeconds;
    }
// GEMREQ-end A_20974-01#intervall
}


// GEMREQ-start A_20974-01#creation,A_20765-02#creation
CFdSigErpManager::CFdSigErpManager(const Configuration& configuration, TslManager& tslManager, HsmPool& hsmPool)
    : TimerJobBase("CFdSigErpManager job", getOcspRequestInterval(configuration))
    , mTslManager(tslManager)
    , mValidationHookId()
    , mHsmPool(hsmPool)
    , mLastProducedAt()
    // the default C.FD.OSIG eRP Signer certificate OCSP grace period is defined in A_20765-02
    , mOcspRequestGracePeriod( std::chrono::seconds(configuration.getIntValue(ConfigurationKey::OCSP_C_FD_SIG_ERP_GRACE_PERIOD)))
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


OcspResponse CFdSigErpManager::getOcspResponseData(const bool forceOcspRequest)
{
    auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    return internalGetOcspResponseData(cFdSigErp, forceOcspRequest);
}


OcspResponsePtr CFdSigErpManager::getOcspResponse()
{
    auto cFdSigErp = mHsmPool.acquire().session().getVauSigCertificate();
    auto responseData = internalGetOcspResponseData(cFdSigErp, false);
    return OcspHelper::stringToOcspResponse(responseData.response);
}


// GEMREQ-start A_20765-02#validation
OcspResponse
CFdSigErpManager::internalGetOcspResponseData(const Certificate& certificate, const bool forceOcspRequest)
{
    try
    {
        auto x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

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
                        : OcspCheckDescriptor::OcspCheckMode::CACHED_ONLY,
                    {std::nullopt, mOcspRequestGracePeriod},
                    {}});

        Expect(responseData.status.certificateStatus == CertificateStatus::good,
               "TslManager::getCertificateOcspResponse() must only return in case of good ocsp status.");

        if (!responseData.fromCache)
        {
            std::unique_lock lock{mLastProducedAtMutex};
            mLastProducedAt = responseData.producedAt;
        }
        else if (forceOcspRequest)
        {
            TLOG(WARNING) << "OCSP request has failed, last successful OCSP response was done at "
                            << model::Timestamp(responseData.receivedAt).toXsDateTime();
        }
        return responseData;
    }
    catch(const TslError& tslError)
    {
        std::unique_lock lock{mLastProducedAtMutex};
        mLastProducedAt = std::chrono::system_clock::time_point{};
        // in this context TslError always means service unavailable error
        throw ExceptionWrapper<TslError>::create({__FILE__, __LINE__}, tslError, HttpStatus::ServiceUnavailable);
    }
}
// GEMREQ-end A_20765-02#validation


void CFdSigErpManager::healthCheck()
{
    std::shared_lock lock{mLastProducedAtMutex};
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
    std::shared_lock lock(mLastProducedAtMutex);

    if (mLastProducedAt == std::chrono::system_clock::time_point{})
    {
        return "never successfully validated";
    }

    return model::Timestamp(mLastProducedAt).toXsDateTime();
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
