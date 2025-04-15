/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_AUTHORIZATIONASSERTION_HXX
#define EPA_LIBRARY_CRYPTO_AUTHORIZATIONASSERTION_HXX

#include "library/crypto/AuthorizedIdentity.hxx"
#include "library/epa/RecordState.hxx"
#include "library/util/BinaryBuffer.hxx"
#include "library/util/Time.hxx"

#include <chrono>
#include <optional>
#include <string>

namespace epa
{

/**
 * Initial support for authorization assertion.
 * Focused for use in TEE protocol, hence only extraction of recordId and homeCommunityId.
 */
class AuthorizationAssertion
{
public:
    enum AuthorizationType
    {
        NO_AUTHORIZATION,
        DOCUMENT_AUTHORIZATION,
        ACCOUNT_AUTHORIZATION,
        AUTHORIZE_REPRESENTATIVE,
    };
};

} // namespace epa

#endif
