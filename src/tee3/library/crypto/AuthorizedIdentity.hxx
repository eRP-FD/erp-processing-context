/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX
#define EPA_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX

#include <string>

namespace epa
{

struct AuthorizedIdentity
{
    enum Type
    {
        IDENTITY_SUBJECT_ID,
        IDENTITY_ORGANIZATION_ID
    };

    std::string identity;
    Type type{};

    bool operator==(const AuthorizedIdentity& other) const
    {
        return type == other.type && identity == other.identity;
    }

    bool operator!=(const AuthorizedIdentity& other) const
    {
        return ! (*this == other);
    }
};

} // namespace epa
#endif
