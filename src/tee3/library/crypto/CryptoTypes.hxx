/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_CRYPTOTYPES_HXX
#define EPA_LIBRARY_CRYPTO_CRYPTOTYPES_HXX

#include "library/crypto/SensitiveDataGuard.hxx"

#include <functional>
#include <optional>

namespace epa
{

using EncryptionKey = SensitiveDataGuard;
using DbIndexHash = std::string;

using EncryptionFunction = std::function<std::string(const std::string& data)>;
using DecryptionFunction = std::function<std::string(const std::string& data)>;

using AuditLogDecryptionFunction =
    std::function<std::string(const std::string& data, const std::optional<int16_t>& keyId)>;

} // namespace epa
#endif
