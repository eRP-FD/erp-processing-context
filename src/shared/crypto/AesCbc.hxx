/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <cstddef>
#include <string>

#include "shared/util/SafeString.hxx"

class AesCbcException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/**
 * Implement AES-CBC encryption/decryption. Requires plaintext to be
 * padded to multiples of `BlockSize`.
 */
template<std::size_t KeyLengthBits>
class AesCbc
{
public:
    static constexpr std::size_t KeyLength = KeyLengthBits / 8;
    static constexpr std::size_t IvLength = 16;
    static constexpr std::size_t BlockSize = 16;

    static std::string encrypt(std::string_view plaintext, const SafeString& key, std::string_view iv);

    static SafeString decrypt(std::string_view ciphertext, const SafeString& key, std::string_view iv);
};

extern template class AesCbc<128>;
extern template class AesCbc<256>;

using AesCbc128 = AesCbc<128>;
using AesCbc256 = AesCbc<256>;
