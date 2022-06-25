#include "erp/pc/CFdSigErpManager.hxx"

#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"
#include "erp/hsm/HsmPool.hxx"


namespace
{
    constexpr int ocspValidationBufferSeconds = 120;
    std::chrono::system_clock::duration getOcspRequestInterval(const Configuration& configuration)
    {
        std::chrono::seconds updateSeconds(
                configuration.getOptionalIntValue(ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD,
                                                  OcspHelper::DEFAULT_OCSP_GRACEPERIOD_SECONDS));

        // The OCSP-Response is valid for some time (updateSeconds),
        // requesting OCSP-Response exactly after this time could lead to a situation
        // where there is no valid cached OCSP-Response.
        // For this reason, if the configured time interval is big enough, there is a 2 minutes time buffer,
        // the timer requests new OCSP-Response 2 minutes before the old one is outdated.
        if (updateSeconds.count() > ocspValidationBufferSeconds * 2)
        {
            updateSeconds -= std::chrono::seconds(ocspValidationBufferSeconds);
        }

        return updateSeconds;
    }
}


CFdSigErpManager::CFdSigErpManager(
    const Configuration& configuration,
    TslManager& tslManager,
    HsmPool& hsmPool)
    : TimerJobBase("CFdSigErpManager job", getOcspRequestInterval(configuration))
    , mMutex()
    , mTslManager(tslManager)
    , mValidationHookId()
    , mHsmPool(hsmPool)
    , mOcspRequestGracePeriod(std::chrono::seconds(
          configuration.getOptionalIntValue(ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD,
                                            OcspHelper::DEFAULT_OCSP_GRACEPERIOD_SECONDS)))
    , mLastSuccess()
    , mLastCheckSuccessfull(false)
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


TrustStore::OcspResponseData
CFdSigErpManager::internalGetOcspResponseData(const Certificate& certificate, const bool forceOcspRequest)
{
    // internal implementation call, it has to be guarded by mutex in caller public/protected methods
    mLastCheckSuccessfull = false;
    try
    {
        // after the Certificate and X509Certificate classes refactored to one class this workaround will no more be necessary,
        // it could be possible to share X509 pointer between classes, but this approach looks to be more safe
        auto x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

        auto responseData =
            mTslManager.getCertificateOcspResponse(
                TslMode::TSL,
                x509Certificate,
                {CertificateType::C_FD_SIG, CertificateType::C_FD_OSIG},
                forceOcspRequest);

        if (responseData.status.certificateStatus == OcspService::CertificateStatus::good)
        {
            mLastCheckSuccessfull = true;
            mLastSuccess = responseData.timeStamp;
        }
        return responseData;
    }
    catch(const TslError& tslError)
    {
        // in this context TslError always means internal server error
        throw TslError(tslError, HttpStatus::InternalServerError);
    }
}


void CFdSigErpManager::healthCheck()
{
    std::lock_guard lock(mMutex);

    if (mLastSuccess == std::chrono::system_clock::time_point{})
    {
        throw std::runtime_error("never successfully validated");
    }

    if (mLastCheckSuccessfull)
    {
        const auto now = std::chrono::system_clock::now();
        if (mLastSuccess + mOcspRequestGracePeriod < now)
        {
            throw std::runtime_error("last validation is too old");
        }
    }
    else
    {
        throw std::runtime_error("last validation has failed");
    }
}


std::string CFdSigErpManager::getLastValidationTimestamp()
{
    std::lock_guard lock(mMutex);

    if (mLastSuccess == std::chrono::system_clock::time_point{})
    {
        return "never successfully validated";
    }

    return model::Timestamp(mLastSuccess).toXsDateTime();
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


void CFdSigErpManager::executeJob()
{
    try
    {
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


void CFdSigErpManager::onFinish()
{
}
