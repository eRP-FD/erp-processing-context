/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/SecureRandomGenerator.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <stdexcept>

namespace epa
{

// GEMREQ-start GS-A_4367, GS-A_4368#SecureRandomGenerator
BinaryBuffer SecureRandomGenerator::generate(std::size_t size)
{
    BinaryBuffer bufferResult(size);

    if (1 != RAND_priv_bytes(bufferResult.data(), bufferResult.sizeAsInt()))
    {
        throw std::runtime_error("CSPRNG failure");
    }

    return bufferResult;
}
// GEMREQ-end GS-A_4367, GS-A_4368#SecureRandomGenerator

// GEMREQ-start A_15362-01#generate
SensitiveDataGuard SecureRandomGenerator::generateSensitiveData(std::size_t size)
{
    return SensitiveDataGuard{SecureRandomGenerator::generate(size)};
}
// GEMREQ-end A_15362-01#generate
} // namespace epa
