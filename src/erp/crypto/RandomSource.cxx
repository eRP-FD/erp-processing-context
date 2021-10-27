/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/RandomSource.hxx"

#include "erp/crypto/SecureRandomGenerator.hxx"

RandomSource& RandomSource::defaultSource()
{
    static SecureRandomGenerator defaultRng;
    return defaultRng;
}
