/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_SECURERANDOMGENERATOR_HXX
#define EPA_LIBRARY_CRYPTO_SECURERANDOMGENERATOR_HXX

#include "shared/crypto/SensitiveDataGuard.hxx"
#include "shared/util/BinaryBuffer.hxx"

#include <cstddef>

namespace epa
{

class SecureRandomGenerator
{
public:
    SecureRandomGenerator() = delete;

    static BinaryBuffer generate(std::size_t size);

    static SensitiveDataGuard generateSensitiveData(std::size_t size);
};

} // namespace epa

#endif
