/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/crypto/SensitiveDataGuard.hxx"

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
        std::size_t derivationLength,
        const SensitiveDataGuard& info = {},
        const SensitiveDataGuard& salt = {});
};
