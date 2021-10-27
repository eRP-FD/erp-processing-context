/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_SECURERANDOMGENERATOR_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_SECURERANDOMGENERATOR_HXX

#include "erp/crypto/RandomSource.hxx"
#include "erp/util/SafeString.hxx"

#include <cstddef>

/**
 * The purpose of this class is the seeding of the internal state of the pseudorandom number
 * generator (PRNG) maintained by the OpenSSL library which is used in our code. We are requiring
 * that the PRNG is seeded, with data that has a sufficient amount entropy, so that the random
 * numbers (bytes) generated were of high quality even if the internal entropy pool of OpenSSL
 * had an initial entropy of 0.
 *
 * GS-A_4367 in gemSpec_Krypt_V2.15.0 requires to provide at least 120 bits of entropy.
 * This is ensured in the definitions of the ClientConfiguration class, which the client is
 * required to use in order to get a valid session. The entropy is calculated in this class.
 * Adding seed data also updates the seed data for the next client session.
 *
 * About the algorithmic recommendations specified by the BSI in the schema AIS-20:
 *
 * OpenSSL's implementation adheres to the guidelines in NIST SP 800-90A Rev. 1, which in turn is
 * recommended by the BSI, as far as the generation of random numbers in concerned.
 *
 * Links:
 *
 * https://www.openssl.org/docs/man1.1.1/man7/RAND_DRBG.html (OpenSSL)
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90Ar1.pdf (NIST)
 * https://www.bsi.bund.de/DE/Themen/ZertifizierungundAnerkennung/Produktzertifizierung/ZertifizierungnachCC/AnwendungshinweiseundInterpretationen/AIS-Liste.html (BSI)
 *
 */
class SecureRandomGenerator
    : public RandomSource
{
public:
    SecureRandomGenerator() = default;

    /**
     * Calls OpenSSL's RAND_add to mix some bytes into the state of the pseudorandom number
     * generator.
     * The more "random" the string the better.
    */
    static void addEntropy (const SafeString& string);

    /**
     * Returns random seed value with size.
     * @return random seed value with the length of size
     */
    static SafeString generate (std::size_t size);

    /**
     * Entropy measure, see: https://en.wikipedia.org/wiki/Entropy_(information_theory)
     * Calculates H(X) = -∑_i ( P(x[i]) * log_2 P(x[i]) ), where
     *     P(x) probability of x in input string, i = length of input, x[i] = each element/byte
     * @param data input data for entropy calculation
     * @return the entropy in bits
    */
    static double shannonEntropy (const std::string_view& data);

    SafeString randomBytes(size_t count) override;

};

#endif
