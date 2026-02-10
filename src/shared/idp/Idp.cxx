/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/idp/Idp.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"


Certificate Idp::getCertificate() const
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


void Idp::setSecondaryCertificate(Certificate secondaryCertificate)
{
    const std::scoped_lock lock(mMutex);

    mSecondaryCertificate.emplace(std::move(secondaryCertificate));
}


std::optional<Certificate> Idp::getSecondaryCertificate() const
{
    const std::scoped_lock lock(mMutex);
    return mSecondaryCertificate;
}


void Idp::resetSecondaryCertificate()
{
    const std::scoped_lock lock(mMutex);
    mSecondaryCertificate.reset();
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
