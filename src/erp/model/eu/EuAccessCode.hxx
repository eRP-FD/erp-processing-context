// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/util/SafeString.hxx"

namespace model
{

class EuAccessCode
{
public:
    explicit EuAccessCode(SafeString&& accessCode);
    const SafeString& toString() const;

    [[nodiscard]] bool operator==(const EuAccessCode& other) const = default;

private:
    SafeString mAccessCode;
};

}// model
