/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

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

bool Idp::isHealthy() const
{
    try
    {
        healthCheck();
    }
    catch (const std::exception&)
    {
        return false;
    }
    return true;
}

void Idp::healthCheck() const
{
    static const auto maxLastUpdateInterval =
        std::chrono::hours(Configuration::instance().getIntValue(ConfigurationKey::IDP_CERTIFICATE_MAX_AGE_HOURS));

    std::lock_guard lock(mMutex);

    if (mLastUpdate == std::chrono::system_clock::time_point{})
    {
        Fail2("never updated", std::runtime_error);
    }

    const auto now = std::chrono::system_clock::now();
    if (mLastUpdate + maxLastUpdateInterval < now)
    {
        Fail2("last update is too old", std::runtime_error);
    }

    if (!mSignerCertificate.has_value())
    {
        Fail2("no valid IDP certificate", std::runtime_error);
    }
}
