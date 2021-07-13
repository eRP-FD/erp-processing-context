#include "erp/crypto/RandomSource.hxx"

#include "erp/crypto/SecureRandomGenerator.hxx"

RandomSource& RandomSource::defaultSource()
{
    static SecureRandomGenerator defaultRng;
    return defaultRng;
}
