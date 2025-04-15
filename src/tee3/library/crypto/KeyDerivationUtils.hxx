/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_KEYDERIVATIONUTILS_HXX
#define EPA_LIBRARY_CRYPTO_KEYDERIVATIONUTILS_HXX

#include "library/crypto/SensitiveDataGuard.hxx"

namespace epa
{

/**
 * Static class containing utilities for performing various types of key derivation.
 */
class KeyDerivationUtils
{
public:
    KeyDerivationUtils() = delete;

    /**
     * Generates an "extract-then-expand" key derivation for the given `key`
     * using HMAC-SHA256 and returns `derivationLength` bytes of the expansion.
     */
    static SensitiveDataGuard performHkdfHmacSha256(
        const SensitiveDataGuard& key,
        size_t derivationLength);
};

} // namespace epa

#endif
