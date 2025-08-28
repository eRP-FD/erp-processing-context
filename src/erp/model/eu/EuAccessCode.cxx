// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "EuAccessCode.hxx"
#include "shared/util/Expect.hxx"

#include <regex>

namespace model
{

EuAccessCode::EuAccessCode(SafeString&& accessCode)
    : mAccessCode(std::move(accessCode))
{
    static const std::regex pattern("^[a-zA-Z0-9]{6}$");
    ModelExpect(std::regex_match(mAccessCode.c_str(), pattern), "Der übermittelte Zugriffscode ist nicht zulässig.");
}

const SafeString& EuAccessCode::toString() const
{
    return mAccessCode;
}

}
