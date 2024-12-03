/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_ELLIPTICCURVEUTILS_HXX
#define EPA_LIBRARY_CRYPTO_ELLIPTICCURVEUTILS_HXX

#include "library/crypto/SensitiveDataGuard.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <memory>
#include <utility> // for std::pair

typedef struct ec_point_st EC_POINT; // NOLINT(readability-identifier-naming) - from OpenSSL

namespace epa
{

class EllipticCurveKey;

/**
 * Static class containing utilities for manipulating
 * elliptic curve primitives and leveraging related EC algorithms.
 */
class EllipticCurveUtils
{
public:
    using EllipticCurveKeyPtr = std::unique_ptr<EllipticCurveKey>;

    /**
     * Describes the formats in which an ECDSA signature may be represented
     */
    enum class SignatureFormat
    {
        ASN1_DER,
        /** Also known as IEEE-P1363. Concatenation of the padded R and S integers. */
        XMLDSIG
    };

    EllipticCurveUtils() = delete;

    /** The NID of the brainpoolp256r1 elliptic curve. */
    static const int BrainpoolP256R1;

    /** The NID of the P-256 elliptic curve. */
    static const int P256;

    /**
     * Performs the "ECC Partial Public-Key Validation Routine" from NIST SP800-56A R3.
     *
     * Tests whether the given EC point is on the brainpool256r1 curve and NOT the point at
     * infinity, or otherwise invalid. Also tests the range of the coordinates.
     *
     * @throw std::runtime_error if any check fails
     */
    static void ensureValidPointOnCurve(const EC_POINT* point, int nid);

    /**
     * Retrieves the public key from the given EllipticCurveKey `key`
     * and returns it as two `BinaryBuffer`s, one for each coordinate.
     * Leading zeros are not omitted.
     */
    static std::pair<BinaryBuffer, BinaryBuffer> getPublicKeyBuffersFromKey(
        const EllipticCurveKey& key);

    /**
     * Retrieves the private key from the given EllipticCurveKey `key`
     * and returns it as a binary `BinaryBuffer`.
     */
    static SensitiveDataGuard getPrivateKeyBufferFromKey(const EllipticCurveKey& key);

    /**
     * Constructs an EllipticCurveKey from the given binary representation of a public key.
     *
     * Performs relevant checks from SP800-56A Rev. 3, the caller does not have to verify the key.
     *
     * @return a public key on the given curve
     * @throw std::runtime_error without detailed explanation if something is off
     */
    static EllipticCurveKeyPtr makeKeyFromPublicKeyBuffers(
        const BinaryBuffer& xCoordinateBuffer,
        const BinaryBuffer& yCoordinateBuffer,
        int nid);

    /**
     * Creates an instance of EllipticCurveKeyPtr based on a privateKeyBuffer.
     * This sets both the private key and public key.
     *
     * Performs relevant checks from SP800-56A Rev. 3, the caller does not have to verify the key.
     *
     * @param privateKeyBuffer serialized integer
     * @return an elliptic key pair on the Brainpool 256 curve
     * @throw std::runtime_error without detailed explanation if something is off
     */
    static EllipticCurveKeyPtr makeKeyFromPrivateKeyBuffer(
        const SensitiveDataGuard& privateKeyBuffer,
        int nid);

    /**
     * Performs the multiplication of the point represented by `publicKey` with the
     * scalar represented by `privateKey` in the `publicKey`'s underlying finite field.
     *
     * Equation: result = privateKey (scalar) * publicKey (point on a curve)
     *
     * The result is yet another point on the same elliptic curve as `publicKey`
     * and is thus represented and returned as an `EllipticCurveKey`.
     */
    static EllipticCurveKeyPtr performSimpleMultiplication(
        const EllipticCurveKey& privateKey,
        const EllipticCurveKey& publicKey);

    /**
     * Performs the multiplication of the point represented by `publicKey` with the
     * scalar represented by `privateKey` in the `publicKey`'s underlying finite field.
     *
     * The resulting point is then arithmetically added
     * (by the same finite field's composition rules) to the result of multiplying the generator
     * point of `publicKey`'s underlying curve with the scalar represented by `ephemeralPrivateKey`.
     *
     * Equation: result = privateKey (scalar) * publicKey (point on a curve) +
     *                    ephemeralPrivateKey (scalar) * G (generator point of the given curve)
     *
     * The result is yet another point on the same elliptic curve as `publicKey`
     * and is thus represented and returned as an `EllipticCurveKey`.
     */
    static EllipticCurveKeyPtr performComposedMultiplication(
        const EllipticCurveKey& privateKey,
        const EllipticCurveKey& publicKey,
        const EllipticCurveKey& ephemeralPrivateKey);

    /**
     * Generates a shared secret between the owner of `privateKey` and the owner of `publicKey`
     * by leveraging the Elliptic Curve Diffie Hellman algorithm.
     *
     * Equation: result = x * (y * G) = y * (x * G), where: x = `privateKey` (ours)
     *                                                      y = peer's private key
     *                                                      y * G = `publicKey` (peer's)
     *                                                      x * G = our own public key
     */
    static SensitiveDataGuard generateDiffieHellmanSharedSecret(
        const EllipticCurveKey& privateKey,
        const EllipticCurveKey& publicKey);

    /**
     * Creates an ASN.1 (DER-encoded) signature of the given message.
     *
     * The digest will be calculated internally using the Hash::sha256 method.
     */
    static BinaryBuffer createAsn1Signature(
        const BinaryBuffer& message,
        const EllipticCurveKey& privateKey);

    /**
     * Tells whether the given `signature` (expected in the `format` format) is
     * correctly verified for the given `message` against the given `publicKey`.
     */
    static bool isSignatureValid(
        const BinaryBuffer& message,
        const EllipticCurveKey& publicKey,
        const BinaryBuffer& signature,
        SignatureFormat format);

    /**
     * Converts the given ECDSA `signature` from the `fromFormat` to the `toFormat`.
     *
     * Formats must be different; given `signature` must be in the `fromFormat` format.
     */
    static BinaryBuffer convertSignatureFormat(
        const BinaryBuffer& signature,
        SignatureFormat fromFormat,
        SignatureFormat toFormat);

private:
    static EllipticCurveKeyPtr performMultiplicationImpl(
        const EllipticCurveKey& privateKey,
        const EllipticCurveKey& publicKey,
        const EllipticCurveKey* ephemeralPrivateKey);
};

} // namespace epa

#endif
