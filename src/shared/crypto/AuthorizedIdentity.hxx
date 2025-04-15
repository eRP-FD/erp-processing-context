/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX
#define E_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX

#include <string>

struct AuthorizedIdentity
{
    enum Type {
        IDENTITY_SUBJECT_ID,
        IDENTITY_ORGANIZATION_ID
    };

    std::string identity;
    Type type;
};

#endif
