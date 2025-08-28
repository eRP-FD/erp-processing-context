// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessPermission.hxx"
#include "shared/util/Expect.hxx"

#include <regex>
#include <utility>

namespace model
{

const std::chrono::hours EuAccessPermission::DEFAULT_VALIDITY_PERIOD = std::chrono::hours{1};

EuAccessPermission::EuAccessPermission(const EuAccessCode& accessCode, const CountryCode& countryCode)
    : EuAccessPermission(accessCode, countryCode, model::Timestamp::now() + DEFAULT_VALIDITY_PERIOD)
{
}

EuAccessPermission::EuAccessPermission(EuAccessCode accessCode, CountryCode countryCode,
                                       const model::Timestamp& validUntil)
    : mAccessCode(std::move(accessCode))
    , mCreatedAt(validUntil - DEFAULT_VALIDITY_PERIOD)
    , mValidUntil(validUntil)
    , mCountryCode(std::move(countryCode))
{
}

const EuAccessCode& EuAccessPermission::getAccessCode() const
{
    return mAccessCode;
}

const CountryCode& EuAccessPermission::getCountryCode() const
{
    return mCountryCode;
}

const Timestamp& EuAccessPermission::getCreatedAt() const
{
    return mCreatedAt;
}

const Timestamp& EuAccessPermission::getValidUntil() const
{
    return mValidUntil;
}

bool EuAccessPermission::isValid(const model::Timestamp& referenceTimestamp) const
{
    return (referenceTimestamp >= mCreatedAt) && (referenceTimestamp <= mValidUntil);
}

}
