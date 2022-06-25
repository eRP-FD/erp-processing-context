/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_ELLIPTICCURVEUTILS_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_ELLIPTICCURVEUTILS_HXX

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/Buffer.hxx"

class SafeString;
/**
 * A collection of helper functions that are used primarily for testing.
 * They are currently located in the erp/ tree because of an interim use in the HSM class.
 * Once that goes away, the EllipticCurveUtils class can be moved to test/mock/.
 *
 * If a function turns out to be used in regular production code, it should be moved to EllipticCurve.
 */
class EllipticCurveUtils
{
public:
    /**
     * Describes the formats in which an ECDSA signature may be represented
     */
    enum class SignatureFormat
    {
        ASN1_DER,
        XMLDSIG
    };

    static shared_EVP_PKEY createPublicKeyHex (
        const std::string& xCoordinateHex,
        const std::string& yCoordinateHex,
        int curveId = NID_brainpoolP256r1);

    static shared_EVP_PKEY createPublicKeyBin (
        const std::string_view& xCoordinateBin,
        const std::string_view& yCoordinateBin,
        int curveId = NID_brainpoolP256r1);

    static shared_EVP_PKEY createPublicKeyBN (
        const shared_BN& xCoordinateBin,
        const shared_BN& yCoordinateBin,
        int curveId = NID_brainpoolP256r1);

    static shared_EVP_PKEY createBrainpoolP256R1PrivateKeyHex (
        const std::string& pComponent);

    static std::tuple<std::string,std::string> getPublicKeyCoordinatesHex (
        const shared_EVP_PKEY& privateKey);

    struct PaddedComponents
    {
        const std::string x;
        const std::string y;
    };
    static PaddedComponents getPaddedXYComponents (const shared_EVP_PKEY& keyPair, const size_t length);

    // pemString - base64 encoded DER content inlcuding header and footer.
    static shared_EVP_PKEY pemToPrivatePublicKeyPair (const SafeString& pemString);
    static shared_EVP_PKEY pemToPublicKey (const SafeString& pemString);

    static shared_EVP_PKEY x962ToPublicKey(const SafeString& derString);
    static std::string publicKeyToPem (const shared_EVP_PKEY& publicKey);
    static std::string publicKeyToX962Der (const shared_EVP_PKEY& publicKey);
    static std::string privateKeyToPem (const shared_EVP_PKEY& privateKey);

    /**
     * Converts the given ECDSA `signature` from the `fromFormat` to the `toFormat`.
     *
     * Formats must be different; given `signature` must be in the `fromFormat` format.
     */
    static util::Buffer convertSignatureFormat(const util::Buffer& signature,
                                               SignatureFormat fromFormat,
                                               SignatureFormat toFormat);
};


#endif
