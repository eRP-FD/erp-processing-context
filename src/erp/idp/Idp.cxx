#include "erp/idp/Idp.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"


const Certificate& Idp::getCertificate(void) const
{
    std::lock_guard lock(mMutex);

    ErpExpect(mSignerCertificate.has_value(), HttpStatus::InternalServerError,
              "IDP certificate has not been initialized");
    return mSignerCertificate.value();
}


void Idp::setCertificate(Certificate&& idpCertificate)
{
    std::lock_guard lock(mMutex);

    mSignerCertificate.emplace(std::move(idpCertificate));
    mLastUpdate = std::chrono::system_clock::now();
}


void Idp::resetCertificate(void)
{
    std::lock_guard lock(mMutex);
    mSignerCertificate.reset();
}


void Idp::healthCheck() const
{
    static const auto updateInterval =
        std::chrono::minutes(Configuration::instance().getIntValue(ConfigurationKey::IDP_UPDATE_INTERVAL_MINUTES));

    std::lock_guard lock(mMutex);

    if (mLastUpdate == std::chrono::system_clock::time_point{})
    {
        throw std::runtime_error("never updated");
    }

    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate + updateInterval * 1.1 < now)
    {
        throw std::runtime_error("last update is too old");
    }

    if (!mSignerCertificate.has_value())
    {
        throw std::runtime_error("no valid IDP certificate");
    }
}
