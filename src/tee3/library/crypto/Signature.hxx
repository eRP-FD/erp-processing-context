/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_SIGNATURE_HXX
#define EPA_LIBRARY_CRYPTO_SIGNATURE_HXX

#include "library/util/BinaryBuffer.hxx"

#include <memory>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

/**
 * This is the class that should be used to handle signatures in the TEE protocol.
 * Both EcSignature, for elliptic curves, and RsaSignatures for RSA, encapsulate implementation details and should,
 * outside tests, not be used directly.
 */
class Signature
{
public:
    virtual ~Signature() = default;

    /**
     * Create new Signature object from binary representation of a signature, i.e. NOT base64 encoded.
     */
    static std::unique_ptr<Signature> from(const BinaryView& data);

    /**
     * Create new Signature object from binary representation of a signature.
     * data is the concatenation of 32byte encodings of R and S (IEEE-P1363)
     */
    static std::unique_ptr<Signature> fromP1363(const BinaryView& data);

    /**
     * At the moment we have to distinguish between these signature formats:
     * - EC_SIMPLE is what OpenSSL writes for an elliptic curve signature (basically an ASN1 sequence of two integers, r and s).
     *   It may be what that Aktor still returns (probably to not break implementations that lag behind).
     * - EC_GEMATIK is EC_SIMPLE with a prefixed sequence of oid NID_ecdsa_with_SHA256. This is what is defined in gemSpec_Krypt 2.16
     *   and is accepted (but not returned) by the Aktor.
     * - RSA is produced by older, but still used, health cards.
     *   The format seems to be a single raw (large = 256bytes) number.
     */
    enum class Format
    {
        EC_SIMPLE,
        EC_GEMATIK,
        RSA,
        Unknown
    };
    static Format detectFormat(const BinaryView& data);


    /**
     * Create a signature by signing `data` with the given `privateKey`.
     */
    static std::unique_ptr<Signature> create(const BinaryView& data, EVP_PKEY& privateKey);

    struct VerificationParameters
    {
        bool isRestrictedToEcdsa = false;
        bool isMgf1SupportedForRsa = true;
    };
    void verifyOrThrow(
        const BinaryView& data,
        EVP_PKEY& publicKey,
        VerificationParameters parameters) const;

    /**
     * Return a serialized form of the `Signature` that is suitable to be included in, say, a TEE handshake request or response.
     */
    virtual BinaryBuffer toBuffer() const = 0;

    /**
     * Return a serialized form of the `Signature` that is suitable to be read by OpenSSL.
     */
    virtual BinaryBuffer toOpenSslBuffer() const = 0;

    /**
     * Return a serialized form of the `Signature` that is suitable to be used in JWT format.
     * It is base64 encoded.
     */
    virtual std::string toJwtBase64String(const size_t signatureSizeBits) const = 0;

    /**
     * Return a serialized form of the `Signature` that is suitable to be used in Challenge verification request to
     * sekIDP.
     */
    virtual std::string toNormalBase64String(const size_t signatureSizeBits) const = 0;

private:
    static void fineTuneKeyContext(
        EVP_PKEY_CTX& keyContext,
        EVP_PKEY& privateKey,
        VerificationParameters verificationParameters);
};


class EcSignature : public Signature
{
public:
    static std::unique_ptr<Signature> create(const BinaryView& data, EC_KEY& privateKey);

    static std::unique_ptr<Signature> from(const BinaryView& data);
    static std::unique_ptr<EcSignature> fromP1363(const BinaryView& data);

    /**
     * Create a new Signature object, based on elliptic curve brainpoolP256r1, from its two r and s parameters.
     */
    EcSignature(shared_BN r, shared_BN s);

    /**
     * Return the R parameter. Not encoded.
     */
    BinaryBuffer getR() const;
    /**
     * Return the S parameter. Not encoded.
     */
    BinaryBuffer getS() const;

    BinaryBuffer toBuffer() const override;
    BinaryBuffer toOpenSslBuffer() const override;
    std::string toJwtBase64String(const size_t signatureSizeBits) const override;
    std::string toNormalBase64String(const size_t signatureSizeBits) const override;

    BinaryBuffer toBufferInSimpleForm() const;
    BinaryBuffer toBufferInGematikForm() const;
    BinaryBuffer toRSBuffer(const size_t signatureSizeBits) const;

private:
    shared_BN mR;
    shared_BN mS;
};


class RsaSignature : public Signature
{
public:
    explicit RsaSignature(BinaryBuffer data);
    ~RsaSignature() override = default;

    static std::unique_ptr<Signature> from(const BinaryView& data);

    BinaryBuffer toBuffer() const override;
    BinaryBuffer toOpenSslBuffer() const override;
    std::string toJwtBase64String(const size_t signatureSizeBits) const override;
    std::string toNormalBase64String(const size_t signatureSizeBits) const override;

private:
    BinaryBuffer mData;
};

} // namespace epa

#endif
