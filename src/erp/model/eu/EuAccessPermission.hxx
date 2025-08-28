// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "EuAccessCode.hxx"
#include "shared/model/CountryCode.hxx"
#include "shared/model/Timestamp.hxx"

namespace model
{

class EuAccessPermission
{
public:
    static const std::chrono::hours DEFAULT_VALIDITY_PERIOD;
    explicit EuAccessPermission(const EuAccessCode& accessCode, const CountryCode& countryCode);
    explicit EuAccessPermission(EuAccessCode accessCode, CountryCode countryCode, const Timestamp& validUntil);

    const EuAccessCode& getAccessCode() const;
    const CountryCode& getCountryCode() const;
    const Timestamp& getCreatedAt() const;
    const Timestamp& getValidUntil() const;
    bool isValid(const model::Timestamp& referenceTimestamp) const;

private:
    EuAccessCode mAccessCode;
    Timestamp mCreatedAt;
    Timestamp mValidUntil;
    CountryCode mCountryCode;
};

}// model
