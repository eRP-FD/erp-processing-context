/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/EllipticCurveUtils.hxx"
#include "library/crypto/EllipticCurveKey.hxx"
#include "library/util/Assert.hxx"
#include "library/util/MemoryUnits.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <stdexcept>
#include <utility>

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Hash.hxx"

namespace epa
{

namespace
{
    using BigNumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
    using BigNumConstPtr = std::unique_ptr<const BIGNUM, void (*)(const BIGNUM*)>;
    using EcPointPtr = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
    using EcGroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;
    using EcdsaSignaturePtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
    using BufferPtr =
        std::unique_ptr<BinaryBuffer::value_type, void (*)(BinaryBuffer::value_type*)>;


    /**
     * Constant describing how large an ECDSA component should be
     * if generated with an underlying curve of 256 bits field size, which is our use case.
     */
    constexpr std::size_t ecdsaComponentSize = 32_B;


    /**
     * Throws an exception unless the given parameter has the expected value of 1.
     *
     * Will be refactored via the introduction of more specific exception types.
     */
    void throwException(int operationResult)
    {
        if (1 != operationResult)
        {
            throw std::runtime_error{"EllipticCurveUtils error"};
        }
    }


    /**
     * Ensures that a given private key is in the interval [1, prime order - 1],
     * where prime order is any given elliptic curve's generator's order.
     *
     * This implements SP800-56A Rev. 3, 5.6.2.1.2 Owner Assurance of Private-Key Validity.
     */
    void ensurePrivateKeyIsValid(const BIGNUM* primeOrder, const BIGNUM* privateKey)
    {
        throwException(primeOrder != nullptr);

        // make sure value is at least 1
        if (BN_is_zero(privateKey) || BN_is_negative(privateKey))
        {
            throw std::runtime_error{"Private key is 0 or negative"};
        }

        // make sure value is smaller than the prime order
        if (0 <= BN_cmp(privateKey, primeOrder))
        {
            throw std::runtime_error{
                "Private key is equal or greater than the curve's prime order"};
        }
    }

    /**
     * Ensures public key is in range. Adapted from official openssl ec_key.c code.
     * Compare https://github.com/openssl/openssl/blob/master/crypto/ec/ec_key.c#L404
     * Check the range of the EC public key.
     * See SP800-56A R3 Section 5.6.2.3.3 (Part 2)
     * i.e.
     *  - If q = odd prime p: Verify that xQ and yQ are integers in the
     *    interval[0, p - 1], OR
     *  - If q = 2m: Verify that xQ and yQ are bit strings of length m bits.
     */
    void ensurePublicKeyIsInRange(const EC_GROUP* group, const EC_POINT* publicKey)
    {
        BigNumPtr x{BN_new(), &BN_clear_free};
        BigNumPtr y{BN_new(), &BN_clear_free};

        throwException(nullptr != group);
        throwException(nullptr != publicKey);

        // Ensure that we are working with odd prime curves so that the checks below apply.
        // For BrainpoolP256, q is fixed to be
        // q = A9FB57DBA1EEA9BC3E660A909D838D718C397AA3B561A6F7901E0E82974856A7 (which is an odd prime)
        // see https://datatracker.ietf.org/doc/html/rfc5639#section-3.4
        //
        // (Note: Replace this with EC_GROUP_get_field_type when it becomes available.
        // Context: https://github.com/openssl/openssl/issues/6275
        // Supporting non-prime curves would also be possible, but at that point we should look at just letting
        // OpenSSL do the full verification: https://github.com/openssl/openssl/blob/fc5888ccb60f33b366972299f30b976c4dc12162/crypto/ec/ec_key.c#L404)
        throwException(
            NID_X9_62_prime_field == EC_METHOD_get_field_type(EC_GROUP_method_of(group)));

        throwException(
            EC_POINT_get_affine_coordinates(group, publicKey, x.get(), y.get(), nullptr));

        throwException(! BN_is_negative(x.get()));
        throwException(BN_cmp(x.get(), EC_GROUP_get0_order(group)) < 0);
        throwException(! BN_is_negative(y.get()));
        throwException(BN_cmp(y.get(), EC_GROUP_get0_order(group)) < 0);
    }

    /**
     * Provides helpers to serialize/unserialize raw OpenSSL signatures
     * to/from BinaryBuffers having various formats.
     *
     * See https://dth01.ibmgcloud.net/jira/browse/EPA-16259 for future refactorization.
     */
    namespace signature_serialization
    {
        namespace detail
        {
            BigNumPtr parseRawComponentFromXmldsigComponent(const BinaryView& component)
            {
                BigNumPtr rawComponentResult{
                    BN_bin2bn(component.data(), sizeAsInt(component), nullptr),
                    [](auto) { /* ownership transferred */ }};

                throwException(nullptr != rawComponentResult);

                return rawComponentResult;
            }


            BinaryBuffer makeXmldsigComponentFromRawComponent(const BIGNUM* rawComponent)
            {
                BinaryBuffer componentResult(ecdsaComponentSize);
                throwException(
                    componentResult.sizeAsInt()
                    == BN_bn2binpad(
                        rawComponent, componentResult.data(), componentResult.sizeAsInt()));

                return componentResult;
            }
        }


        BinaryBuffer serializeIntoAsn1(const ECDSA_SIG* rawSignature)
        {
            BinaryBuffer::value_type* asn1RawBuffer{};
            auto asn1RawBufferLength{i2d_ECDSA_SIG(rawSignature, &asn1RawBuffer)};

            BufferPtr asn1Buffer{asn1RawBuffer, [](auto rawBuffer) { OPENSSL_free(rawBuffer); }};

            throwException(0 < asn1RawBufferLength);
            throwException(nullptr != asn1Buffer);

            return BinaryBuffer(asn1Buffer.get(), gsl::narrow<size_t>(asn1RawBufferLength));
        }


        BinaryBuffer serializeIntoXmldsig(const ECDSA_SIG* rawSignature)
        {
            BigNumConstPtr rawComponentR{
                ECDSA_SIG_get0_r(rawSignature), [](auto) { /* refcount not increased */ }};
            throwException(nullptr != rawComponentR);

            BigNumConstPtr rawComponentS{
                ECDSA_SIG_get0_s(rawSignature), [](auto) { /* refcount not increased */ }};
            throwException(nullptr != rawComponentS);

            return BinaryBuffer::concatenate(
                detail::makeXmldsigComponentFromRawComponent(rawComponentR.get()),
                detail::makeXmldsigComponentFromRawComponent(rawComponentS.get()));
        }


        EcdsaSignaturePtr unserializeFromAsn1(const BinaryView& signature)
        {
            const auto* rawBufferSignature = signature.data();
            EcdsaSignaturePtr rawSignatureResult{
                d2i_ECDSA_SIG(nullptr, &rawBufferSignature, sizeAsLong(signature)),
                &ECDSA_SIG_free};

            throwException(nullptr != rawSignatureResult);

            return rawSignatureResult;
        }


        EcdsaSignaturePtr unserializeFromXmldsig(const BinaryView& signature)
        {
            const auto signatureSize = signature.size();
            if (signatureSize % 2)
            {
                throw std::invalid_argument{"Given signature is not in XMLDSIG format"};
            }

            EcdsaSignaturePtr rawSignatureResult{ECDSA_SIG_new(), &ECDSA_SIG_free};
            throwException(nullptr != rawSignatureResult);

            const BinaryView firstHalf = signature.first(signatureSize / 2);
            const BinaryView secondHalf = signature.last(signatureSize / 2);

            throwException(ECDSA_SIG_set0(
                rawSignatureResult.get(),
                detail::parseRawComponentFromXmldsigComponent(firstHalf).get(),
                detail::parseRawComponentFromXmldsigComponent(secondHalf).get()));

            return rawSignatureResult;
        }
    }


    /**
     * Serializes the given OpenSSL `rawSignature` into
     * an BinaryBuffer having the format according to `format`.
     */
    BinaryBuffer serializeSignature(
        const EcdsaSignaturePtr& rawSignature,
        EllipticCurveUtils::SignatureFormat format)
    {
        switch (format)
        {
            case EllipticCurveUtils::SignatureFormat::ASN1_DER:
            {
                return signature_serialization::serializeIntoAsn1(rawSignature.get());
            }

            case EllipticCurveUtils::SignatureFormat::XMLDSIG:
            {
                return signature_serialization::serializeIntoXmldsig(rawSignature.get());
            }

            default:
            {
                throw std::invalid_argument{"Unknown signature format"};
            }
        }
    }


    /**
     * Unserializes the given signature (having the format according to `format`)
     * to a raw OpenSSL raw signature.
     */
    EcdsaSignaturePtr unserializeSignature(
        const BinaryBuffer& signature,
        EllipticCurveUtils::SignatureFormat format)
    {
        switch (format)
        {
            case EllipticCurveUtils::SignatureFormat::ASN1_DER:
            {
                return signature_serialization::unserializeFromAsn1(signature);
            }

            case EllipticCurveUtils::SignatureFormat::XMLDSIG:
            {
                return signature_serialization::unserializeFromXmldsig(signature);
            }

            default:
            {
                throw std::invalid_argument{"Unknown signature format"};
            }
        }
    }
}


const int EllipticCurveUtils::BrainpoolP256R1 = NID_brainpoolP256r1;
const int EllipticCurveUtils::P256 = NID_X9_62_prime256v1;


// GEMREQ-start A_17903#ensureValidPointOnCurve
void EllipticCurveUtils::ensureValidPointOnCurve(const EC_POINT* point, int nid)
{
    const EcGroupPtr curve{EC_GROUP_new_by_curve_name(nid), &EC_GROUP_free};
    AssertOpenSsl(curve) << "Could not load elliptic curve with nid: " << nid;

    // From SP800-56A Rev. 3, Section 5.6.2.3.4, "ECC Partial Public-Key Validation Routine":
    // (See https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Ar3.pdf)

    // 5.6.2.3.4 (Step 1): Q != infinity
    throwException(! EC_POINT_is_at_infinity(curve.get(), point));

    // 5.6.2.3.4 (Step 2) Test if the point is in range
    ensurePublicKeyIsInRange(curve.get(), point);

    // 5.6.2.3.4 (Step 3) is the point on the elliptic curve
    throwException(EC_POINT_is_on_curve(curve.get(), point, nullptr));
}
// GEMREQ-end A_17903#ensureValidPointOnCurve


std::pair<BinaryBuffer, BinaryBuffer> EllipticCurveUtils::getPublicKeyBuffersFromKey(
    const EllipticCurveKey& key)
{
    if (! key.hasPublicKey())
    {
        throw std::invalid_argument{"Given elliptic curve key hasn't a public key"};
    }

    BigNumPtr xCoordBigNum{BN_new(), &BN_free};
    throwException(nullptr != xCoordBigNum);

    BigNumPtr yCoordBigNum{BN_new(), &BN_free};
    throwException(nullptr != yCoordBigNum);

    throwException(EC_POINT_get_affine_coordinates(
        key.getGroup(), key.getPublicKey(), xCoordBigNum.get(), yCoordBigNum.get(), nullptr));

    BinaryBuffer xCoordBuffer(key.getFieldSize());
    throwException(
        xCoordBuffer.sizeAsInt()
        == BN_bn2binpad(xCoordBigNum.get(), xCoordBuffer.data(), xCoordBuffer.sizeAsInt()));

    BinaryBuffer yCoordBuffer(key.getFieldSize());
    throwException(
        yCoordBuffer.sizeAsInt()
        == BN_bn2binpad(yCoordBigNum.get(), yCoordBuffer.data(), yCoordBuffer.sizeAsInt()));

    return std::make_pair(std::move(xCoordBuffer), std::move(yCoordBuffer));
}


SensitiveDataGuard EllipticCurveUtils::getPrivateKeyBufferFromKey(const EllipticCurveKey& key)
{
    if (! key.hasPrivateKey())
    {
        throw std::invalid_argument{"Given elliptic curve key hasn't a private key"};
    }

    SensitiveDataGuard privateKeyBuffer{
        BinaryBuffer(gsl::narrow<size_t>(BN_num_bytes(key.getPrivateKey())))};
    // Note: We don't really need to pad the value here, but this function makes it more obvious that we are not writing
    // more bytes than we have.
    throwException(
        privateKeyBuffer.sizeAsInt()
        == BN_bn2binpad(
            key.getPrivateKey(), privateKeyBuffer.data(), privateKeyBuffer.sizeAsInt()));
    return privateKeyBuffer;
}


// GEMREQ-start A_17903#makeKeyFromPublicKeyBuffers
EllipticCurveUtils::EllipticCurveKeyPtr EllipticCurveUtils::makeKeyFromPublicKeyBuffers(
    const BinaryBuffer& xCoordinateBuffer,
    const BinaryBuffer& yCoordinateBuffer,
    int nid)
{
    auto keyResult = std::make_unique<EllipticCurveKey>(nid);
    auto coordinateSize = keyResult->getFieldSize();

    if (xCoordinateBuffer.size() != coordinateSize || yCoordinateBuffer.size() != coordinateSize)
    {
        throw std::invalid_argument{"Public key buffers have wrong size"};
    }

    BigNumPtr xCoordinate{BN_new(), &BN_free};
    throwException(nullptr != xCoordinate);
    throwException(
        nullptr
        != BN_bin2bn(
            xCoordinateBuffer.data(), gsl::narrow<int>(coordinateSize), xCoordinate.get()));

    BigNumPtr yCoordinate{BN_new(), &BN_free};
    throwException(nullptr != yCoordinate);
    throwException(
        nullptr
        != BN_bin2bn(
            yCoordinateBuffer.data(), gsl::narrow<int>(coordinateSize), yCoordinate.get()));

    EcPointPtr publicKeyResult{EC_POINT_new(keyResult->getGroup()), &EC_POINT_free};
    throwException(nullptr != publicKeyResult);

    throwException(EC_POINT_set_affine_coordinates(
        keyResult->getGroup(),
        publicKeyResult.get(),
        xCoordinate.get(),
        yCoordinate.get(),
        nullptr));

    ensureValidPointOnCurve(publicKeyResult.get(), nid);

    keyResult->setPublicKey(publicKeyResult.get());

    return keyResult;
}
// GEMREQ-end A_17903#makeKeyFromPublicKeyBuffer

EllipticCurveUtils::EllipticCurveKeyPtr EllipticCurveUtils::makeKeyFromPrivateKeyBuffer(
    const SensitiveDataGuard& privateKeyBuffer,
    int nid)
{
    auto keyResult = std::make_unique<EllipticCurveKey>(nid);
    if (privateKeyBuffer.size() > keyResult->getFieldSize())
    {
        throw std::invalid_argument{"Private key buffer too large"};
    }

    BigNumPtr privateKey{BN_new(), &BN_clear_free};
    throwException(nullptr != privateKey);
    throwException(
        nullptr
        != BN_bin2bn(privateKeyBuffer.data(), privateKeyBuffer.sizeAsInt(), privateKey.get()));


    ensurePrivateKeyIsValid(EC_GROUP_get0_order(keyResult->getGroup()), privateKey.get());

    EcPointPtr publicKey{EC_POINT_new(keyResult->getGroup()), &EC_POINT_free};
    throwException(nullptr != publicKey);

    // derive public key by multiplying private key with public base point G of domain
    throwException(EC_POINT_mul(
        keyResult->getGroup(), publicKey.get(), privateKey.get(), nullptr, nullptr, nullptr));

    ensureValidPointOnCurve(publicKey.get(), nid);

    keyResult->setKeyPair(privateKey.get(), publicKey.get());

    return keyResult;
}


EllipticCurveUtils::EllipticCurveKeyPtr EllipticCurveUtils::performSimpleMultiplication(
    const EllipticCurveKey& privateKey,
    const EllipticCurveKey& publicKey)
{
    return performMultiplicationImpl(privateKey, publicKey, nullptr);
}


EllipticCurveUtils::EllipticCurveKeyPtr EllipticCurveUtils::performComposedMultiplication(
    const EllipticCurveKey& privateKey,
    const EllipticCurveKey& publicKey,
    const EllipticCurveKey& ephemeralPrivateKey)
{
    return performMultiplicationImpl(privateKey, publicKey, &ephemeralPrivateKey);
}


SensitiveDataGuard EllipticCurveUtils::generateDiffieHellmanSharedSecret(
    const EllipticCurveKey& privateKey,
    const EllipticCurveKey& publicKey)
{
    SensitiveDataGuard dhSecretResult{BinaryBuffer(publicKey.getFieldSize())};
    throwException(
        dhSecretResult.sizeAsInt()
        == ECDH_compute_key(
            dhSecretResult.data(),
            dhSecretResult.size(),
            publicKey.getPublicKey(),
            privateKey.getNativeHandle(),
            nullptr));

    return dhSecretResult;
}


BinaryBuffer EllipticCurveUtils::createAsn1Signature(
    const BinaryBuffer& message,
    const EllipticCurveKey& privateKey)
{
    const auto hashedMessage = Hash::sha256(message.getString());

    // Make room for the maximum length of a DER encoded ECDSA signature created with the given key.
    auto signatureSize = gsl::narrow<unsigned int>(ECDSA_size(privateKey.getNativeHandle()));
    BinaryBuffer signature(signatureSize);
    throwException(ECDSA_sign(
        0,
        reinterpret_cast<const unsigned char*>(hashedMessage.data()),
        static_cast<int>(hashedMessage.size()),
        signature.data(),
        &signatureSize,
        privateKey.getNativeHandle()));
    // ...now shrink the signature down to the space we really needed.
    signature.shrinkToSize(signatureSize);
    return signature;
}


EllipticCurveUtils::EllipticCurveKeyPtr EllipticCurveUtils::performMultiplicationImpl(
    const EllipticCurveKey& privateKey,
    const EllipticCurveKey& publicKey,
    const EllipticCurveKey* ephemeralPrivateKey)
{
    // Ensure that all two or three given keys use the same curve.
    const EC_GROUP* privateKeyGroup = EC_KEY_get0_group(privateKey.getNativeHandle());
    const int privateKeyNid = EC_GROUP_get_curve_name(privateKeyGroup);
    const EC_GROUP* publicKeyGroup = EC_KEY_get0_group(publicKey.getNativeHandle());
    const int publicKeyNid = EC_GROUP_get_curve_name(publicKeyGroup);
    if (privateKeyNid != publicKeyNid)
        throw std::invalid_argument{"Trying to multiply keys from different curves"};

    if (ephemeralPrivateKey)
    {
        const EC_GROUP* ephemeralKeyGroup =
            EC_KEY_get0_group(ephemeralPrivateKey->getNativeHandle());
        const int ephemeralKeyNid = EC_GROUP_get_curve_name(ephemeralKeyGroup);
        if (privateKeyNid != ephemeralKeyNid)
            throw std::invalid_argument{"Trying to multiply keys from different curves"};
    }

    auto keyResult = std::make_unique<EllipticCurveKey>(privateKeyNid);

    EcPointPtr multiplicationResult{EC_POINT_new(keyResult->getGroup()), &EC_POINT_free};
    throwException(nullptr != multiplicationResult);

    keyResult->computeMultiplicationTable();
    throwException(EC_POINT_mul(
        keyResult->getGroup(),
        multiplicationResult.get(),
        ephemeralPrivateKey ? ephemeralPrivateKey->getPrivateKey() : nullptr,
        publicKey.getPublicKey(),
        privateKey.getPrivateKey(),
        nullptr));

    ensureValidPointOnCurve(multiplicationResult.get(), privateKeyNid);

    keyResult->setPublicKey(multiplicationResult.get());

    return keyResult;
}


// GEMREQ-start A_17207#isSignatureValid
bool EllipticCurveUtils::isSignatureValid(
    const BinaryBuffer& message,
    const EllipticCurveKey& publicKey,
    const BinaryBuffer& signature,
    SignatureFormat format)
{
    const auto hashedMessage = Hash::sha256(message.getString());

    return ECDSA_do_verify(
        reinterpret_cast<const unsigned char*>(hashedMessage.data()),
        static_cast<int>(hashedMessage.size()),
        unserializeSignature(signature, format).get(),
        publicKey.getNativeHandle());
}
// GEMREQ-end A_17207#isSignatureValid


BinaryBuffer EllipticCurveUtils::convertSignatureFormat(
    const BinaryBuffer& signature,
    SignatureFormat fromFormat,
    SignatureFormat toFormat)
{
    if (fromFormat == toFormat)
    {
        throw std::logic_error{"Source and destination signature formats cannot be the same"};
    }

    return serializeSignature(unserializeSignature(signature, fromFormat), toFormat);
}

} // namespace epa
