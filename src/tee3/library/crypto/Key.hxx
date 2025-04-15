/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_KEY_HXX
#define EPA_LIBRARY_CRYPTO_KEY_HXX

#include <string>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

/**
 * Collection of functions around private and/or public keys.
 *
 * The keys themselves are represented as shared_EVP_PKEY. Class Key provides static functions that
 * operate on shared_EVP_PKEY and does not have a state.
 * There is also a lot of overlap with EllipticCurveKey. [open for refactoring]
 */
class Key
{
public:
    static std::string privateKeyToPemString(const shared_EVP_PKEY& key);
    static shared_EVP_PKEY privateKeyFromPemString(const std::string& pemString);
    static shared_EVP_PKEY privateKeyFromDer(const std::string& derString);

    static std::string publicKeyToPemString(const shared_EVP_PKEY& key);
    static shared_EVP_PKEY publicKeyFromPemString(const std::string& pemString);

    /**
     * Convert the public key of an EC key pair to X9.62 format encoded as BER stream.
     * See https://dth01.ibmgcloud.net/confluence/pages/viewpage.action?pageId=40577291 section "X9.62" for details.
     */
    static std::string publicKeyToX962DerString(const shared_EVP_PKEY& keyPair);

    /**
     * Convert a public key in X9.62 format, encoded as BER stream back into an openssl EC key pair.
     */
    static shared_EVP_PKEY publicKeyFromX962Der(const std::string& derString);

    struct PaddedComponents
    {
        const std::string x;
        const std::string y;
    };

    /**
     * Returns the X and Y components of the public key of the given key pair as binary strings,
     * padded to the given number of bytes.
     */
    static PaddedComponents getPaddedXYComponents(const EVP_PKEY& keyPair, const size_t length);

    static shared_EVP_PKEY createPublicKeyBin(
        const std::string_view& xCoordinateBin,
        const std::string_view& yCoordinateBin,
        int curveId = NID_brainpoolP256r1);
    static shared_EVP_PKEY createPublicKeyBN(
        const shared_BN& xCoordinateBin,
        const shared_BN& yCoordinateBin,
        int curveId = NID_brainpoolP256r1);

    static size_t getAlgorithmBitCount(const EVP_PKEY& keyPair);
};

} // namespace epa

#endif
