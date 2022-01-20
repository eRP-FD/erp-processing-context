#include "erp/pc/CFdSigErpManager.hxx"

#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"


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
    std::shared_ptr<TslManager> mTslManager)
    : TimerJobBase("CFdSigErpManager job", getOcspRequestInterval(configuration))
    , mMutex()
    , mCFdSigErp(Certificate::fromPemString(configuration.getStringValue(ConfigurationKey::C_FD_SIG_ERP)))
    , mX509Certificate()
    , mTslManager(std::move(mTslManager))
    , mValidationHookId()
    , mOcspRequestGracePeriod(std::chrono::seconds(
          configuration.getOptionalIntValue(ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD,
                                            OcspHelper::DEFAULT_OCSP_GRACEPERIOD_SECONDS)))
    , mLastSuccess()
    , mLastCheckSuccessfull(false)
{
    Expect(mCFdSigErp.toX509() != nullptr, "Valid C.FD.SIG eRP Certificate must be provided.");

    // after the Certificate and X509Certificate classes refactored to one class this workaround will no more be necessary,
    // it could be possible to share X509 pointer between classes, but this approach looks to be more safe
    mX509Certificate = X509Certificate::createFromBase64(mCFdSigErp.toDerBase64String());

    // trigger the first execution synchronously
    executeJob();

    // If TSL is updated the validation has to be done again
    if (mTslManager != nullptr)
    {
        mValidationHookId = mTslManager->addPostUpdateHook([this]{getOcspResponseData(true);});
    }

    start();
}


CFdSigErpManager::~CFdSigErpManager()
{
    if (mTslManager != nullptr && mValidationHookId.has_value())
    {
        mTslManager->disablePostUpdateHook(*mValidationHookId);
    }

    shutdown();
}


const Certificate& CFdSigErpManager::getCertificate()
{
    std::lock_guard lock(mMutex);

    // triggers certificate validation if necessary
    internalGetOcspResponseData(false);

    return mCFdSigErp;
}


std::optional<TrustStore::OcspResponseData> CFdSigErpManager::getOcspResponseData(const bool forceOcspRequest)
{
    std::lock_guard lock(mMutex);
    return internalGetOcspResponseData(forceOcspRequest);
}



OcspResponsePtr CFdSigErpManager::getOcspResponse()
{
    std::lock_guard lock(mMutex);

    const std::optional<TrustStore::OcspResponseData> responseData = internalGetOcspResponseData(false);
    if (responseData.has_value())
    {
        return OcspHelper::stringToOcspResponse(responseData->response);
    }

    return {};
}


std::optional<TrustStore::OcspResponseData> CFdSigErpManager::internalGetOcspResponseData(const bool forceOcspRequest)
{
    // internal implementation call, it has to be guarded by mutex in caller public/protected methods
    if (mTslManager != nullptr)
    {
        mLastCheckSuccessfull = false;
        const std::optional<TrustStore::OcspResponseData> responseData =
            mTslManager->getCertificateOcspResponse(TslMode::TSL, mX509Certificate, {CertificateType::C_FD_SIG}, forceOcspRequest);
        if (responseData.has_value() && responseData->status.certificateStatus == OcspService::CertificateStatus::good)
        {
            mLastCheckSuccessfull = true;
            mLastSuccess = responseData->timeStamp;
        }

        return responseData;
    }

    return {};
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


void CFdSigErpManager::onStart()
{
}


void CFdSigErpManager::executeJob()
{
    try
    {
        std::lock_guard lock(mMutex);
        internalGetOcspResponseData(true);
    }
    catch (const TslError& e)
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
