/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/RandomSource.hxx"

#include "shared/crypto/SecureRandomGenerator.hxx"

RandomSource& RandomSource::defaultSource()
{
    static SecureRandomGenerator defaultRng;
    return defaultRng;
}