#include "erp/crypto/EllipticCurveUtils.hxx"

#include "erp/crypto/EllipticCurve.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/String.hxx"


namespace
{
    using BigNumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
    using BigNumConstPtr = std::unique_ptr<const BIGNUM, void (*)(const BIGNUM*)>;
    using EcdsaSignaturePtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
    using BufferPtr = std::unique_ptr<util::Buffer::value_type,
        void (*)(util::Buffer::value_type*)>;


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
    void throwExceptionIfNotOne(int operationResult)
    {
        if (1 != operationResult)
        {
            throw std::runtime_error{"EllipticCurveUtils error"};
        }
    }

    /**
     * Provides helpers to serialize/unserialize raw OpenSSL signatures
     * to/from util::Buffers having various formats.
     *
     * See https://gcloud.rth-de.ihost.com/jira/browse/EPA-16259 for future refactorization.
     */
    namespace signatureSerialization
    {
        namespace detail
        {
            BigNumPtr parseRawComponentFromXmldsigComponent(const util::Buffer& component)
            {
                BigNumPtr rawComponentResult{BN_bin2bn(util::bufferToRaw(component),
                                                       gsl::narrow_cast<uint16_t>(component.size()),
                                                       nullptr),
                                             [](auto) { /* ownership transferred */ }};

                throwExceptionIfNotOne(nullptr != rawComponentResult);

                return rawComponentResult;
            }


            util::Buffer makeXmldsigComponentFromRawComponent(const BigNumConstPtr& rawComponent)
            {
                util::Buffer componentResult(ecdsaComponentSize);
                throwExceptionIfNotOne(gsl::narrow<int>(componentResult.size()) ==
                                       BN_bn2binpad(rawComponent.get(),
                                                    util::bufferToRaw(componentResult),
                                                    gsl::narrow_cast<uint16_t>(componentResult.size())));

                return componentResult;
            }
        }


        util::Buffer serializeIntoAsn1(const EcdsaSignaturePtr& rawSignature)
        {
            util::Buffer::value_type* asn1RawBuffer{};
            auto asn1RawBufferLength{i2d_ECDSA_SIG(rawSignature.get(), &asn1RawBuffer)};

            BufferPtr asn1Buffer{asn1RawBuffer,
                                 [](auto rawBuffer)
                                 {
                                   OPENSSL_free(rawBuffer);
                                 }};

            throwExceptionIfNotOne(0 < asn1RawBufferLength);
            throwExceptionIfNotOne(nullptr != asn1Buffer);

            return util::rawToBuffer(asn1Buffer.get(), asn1RawBufferLength);
        }


        util::Buffer serializeIntoXmldsig(const EcdsaSignaturePtr& rawSignature)
        {
            BigNumConstPtr rawComponentR{ECDSA_SIG_get0_r(rawSignature.get()),
                                         [](auto) { /* refcount not increased */ }};
            throwExceptionIfNotOne(nullptr != rawComponentR);

            BigNumConstPtr rawComponentS{ECDSA_SIG_get0_s(rawSignature.get()),
                                         [](auto) { /* refcount not increased */ }};
            throwExceptionIfNotOne(nullptr != rawComponentS);

            return util::concatenateBuffers(
                detail::makeXmldsigComponentFromRawComponent(rawComponentR),
                detail::makeXmldsigComponentFromRawComponent(rawComponentS));
        }


        EcdsaSignaturePtr unserializeFromAsn1(const util::Buffer& signature)
        {
            auto rawBufferSignature = util::bufferToRaw(signature);
            EcdsaSignaturePtr rawSignatureResult{d2i_ECDSA_SIG(nullptr,
                                                               &rawBufferSignature,
                                                               gsl::narrow_cast<uint16_t>(signature.size())),
                                                 ECDSA_SIG_free};

            throwExceptionIfNotOne(nullptr != rawSignatureResult);

            return rawSignatureResult;
        }


        EcdsaSignaturePtr unserializeFromXmldsig(const util::Buffer& signature)
        {
            const auto signatureSize = signature.size();
            if (signatureSize % 2)
            {
                throw std::invalid_argument{"Given signature is not in XMLDSIG format"};
            }

            EcdsaSignaturePtr rawSignatureResult{ECDSA_SIG_new(), ECDSA_SIG_free};
            throwExceptionIfNotOne(nullptr != rawSignatureResult);

            const auto componentBorderIterator = signature.cbegin() + (signatureSize / 2);
            throwExceptionIfNotOne(
                ECDSA_SIG_set0(rawSignatureResult.get(),
                               detail::parseRawComponentFromXmldsigComponent(
                                   util::Buffer{signature.cbegin(), componentBorderIterator})
                                   .get(),
                               detail::parseRawComponentFromXmldsigComponent(
                                   util::Buffer{componentBorderIterator, signature.cend()})
                                   .get()));

            return rawSignatureResult;
        }
    }

    /**
     * Serializes the given OpenSSL `rawSignature` into
     * an util::Buffer having the format according to `format`.
     */
    util::Buffer serializeSignature(const EcdsaSignaturePtr& rawSignature,
                                    EllipticCurveUtils::SignatureFormat format)
    {
        switch (format)
        {
            case EllipticCurveUtils::SignatureFormat::ASN1_DER:
            {
                return signatureSerialization::serializeIntoAsn1(rawSignature);
            }

            case EllipticCurveUtils::SignatureFormat::XMLDSIG:
            {
                return signatureSerialization::serializeIntoXmldsig(rawSignature);
            }
        }

        throw std::invalid_argument{"Unknown signature format"};
    }


/**
     * Unserializes the given signature (having the format according to `format`)
     * to a raw OpenSSL raw signature.
     */
    EcdsaSignaturePtr unserializeSignature(const util::Buffer& signature,
                                           EllipticCurveUtils::SignatureFormat format)
    {
        switch (format)
        {
            case EllipticCurveUtils::SignatureFormat::ASN1_DER:
            {
                return signatureSerialization::unserializeFromAsn1(signature);
            }

            case EllipticCurveUtils::SignatureFormat::XMLDSIG:
            {
                return signatureSerialization::unserializeFromXmldsig(signature);
            }
        }

        throw std::invalid_argument{"Unknown signature format"};
    }
}


shared_EVP_PKEY EllipticCurveUtils::createPublicKeyHex (
    const std::string& xCoordinateHex,
    const std::string& yCoordinateHex,
    const int curveId)
{
    OpenSslExpect(xCoordinateHex.size() <= 64, "x coordinate too long, " + std::to_string(xCoordinateHex.size()) + " but must not be longer than 32");
    OpenSslExpect(yCoordinateHex.size() <= 64, "y coordinate too long, " + std::to_string(yCoordinateHex.size()) + " but must not be longer than 32");

    auto x = shared_BN::make();
    size_t count = BN_hex2bn(x.getP(), xCoordinateHex.c_str());
    OpenSslExpect(count == xCoordinateHex.size(), "conversion of x component failed");

    auto y = shared_BN::make();
    count = BN_hex2bn(y.getP(), yCoordinateHex.c_str());
    OpenSslExpect(count == yCoordinateHex.size(), "conversion of y component failed");

    return createPublicKeyBN(x, y, curveId);
}


shared_EVP_PKEY EllipticCurveUtils::createPublicKeyBin (
    const std::string_view& xCoordinateBin,
    const std::string_view& yCoordinateBin,
    const int curveId)
{
    OpenSslExpect(xCoordinateBin.size() <= 32, "x coordinate too long, " + std::to_string(xCoordinateBin.size()) + " but must not be longer than 32");
    OpenSslExpect(yCoordinateBin.size() <= 32, "y coordinate too long, " + std::to_string(yCoordinateBin.size()) + " but must not be longer than 32");

    auto x = shared_BN::make();
    auto result = BN_bin2bn(reinterpret_cast<const unsigned char*>(xCoordinateBin.data()), static_cast<int>(xCoordinateBin.size()), x);
    OpenSslExpect(result==x.get(), "conversion of x component failed");

    auto y = shared_BN::make();
    result = BN_bin2bn(reinterpret_cast<const unsigned char*>(yCoordinateBin.data()), static_cast<int>(yCoordinateBin.size()), y);
    OpenSslExpect(result==y.get(), "conversion of y component failed");

    return createPublicKeyBN(x, y, curveId);
}


shared_EVP_PKEY EllipticCurveUtils::createPublicKeyBN (
    const shared_BN& xCoordinate,
    const shared_BN& yCoordinate,
    const int curveId)
{
    auto key = shared_EC_KEY::make(EC_KEY_new_by_curve_name(curveId));
    OpenSslExpect(key.isSet(), "could not create new EC key on elliptic curve");
    EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
    int status = EC_KEY_generate_key(key);
    OpenSslExpect(status == 1, "generating EVP key from EC key failed");

    auto* group = EC_KEY_get0_group(key);
    OpenSslExpect(group!=nullptr, "could not get group from elliptic curve key");

    auto publicKey = shared_EC_POINT::make(EC_POINT_new(group));
    int result = EC_POINT_set_affine_coordinates(group, publicKey, xCoordinate, yCoordinate, nullptr);
    OpenSslExpect(result == 1, "could not create public key from x,y components");

    result = EC_KEY_set_public_key(key, publicKey);
    OpenSslExpect(result == 1, "could not create ec key public key");

    auto pkey = shared_EVP_PKEY::make();

    status = EVP_PKEY_set1_EC_KEY(pkey, key);
    OpenSslExpect(status == 1, "could not convert key into EC key");

    return pkey;
}


shared_EVP_PKEY EllipticCurveUtils::createBrainpoolP256R1PrivateKeyHex (
    const std::string& pComponent)
{
    auto key = shared_EC_KEY::make(EC_KEY_new_by_curve_name(NID_brainpoolP256r1));
    OpenSslExpect(key.isSet(), "could not create new EC key on brainpoolP256R1 curve");
    EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
    int status = EC_KEY_generate_key(key);
    OpenSslExpect(status == 1, "generating EVP key from EC key failed");

    auto* group = EC_KEY_get0_group(key);
    OpenSslExpect(group!=nullptr, "could not get group from elliptic curve key");

    auto p = shared_BN::make();
    size_t count = BN_hex2bn(p.getP(), pComponent.c_str());
    OpenSslExpect(count==pComponent.size(), "conversion of hex private key failed");

    int result = EC_KEY_set_private_key(key, p);
    OpenSslExpect(result == 1, "could not create ec private key");

    auto publicKey = shared_EC_POINT::make(EC_POINT_new(group));
    result = EC_POINT_mul(group, publicKey, p, nullptr, nullptr, nullptr);
    OpenSslExpect(result == 1, "could not create ec public key");
    result = EC_KEY_set_public_key(key, publicKey);
    OpenSslExpect(result == 1, "could not set ec public key");

    auto pkey = shared_EVP_PKEY::make();
    status = EVP_PKEY_set1_EC_KEY(pkey, key);
    OpenSslExpect(status == 1, "could not convert key into EC key");

    return pkey;
}


std::tuple<std::string,std::string> EllipticCurveUtils::getPublicKeyCoordinatesHex (
    const shared_EVP_PKEY& privateKey)
{
    auto* key = EVP_PKEY_get0_EC_KEY(privateKey.removeConst());
    auto* ecPoint = EC_KEY_get0_public_key(key);
    auto* group = EC_KEY_get0_group(key);
    auto x = shared_BN::make();
    auto y = shared_BN::make();
    EC_POINT_get_affine_coordinates(group, ecPoint, x, y, nullptr);
    return {String::toLower(BN_to_hexString(x)), String::toLower(BN_to_hexString(y))};
}


EllipticCurveUtils::PaddedComponents EllipticCurveUtils::getPaddedXYComponents (const shared_EVP_PKEY& keyPair, const size_t length)
{
    // Limit the padding length so that conversion from size_t to int does not make a problem.
    // If 32 is not sufficient for a future use case, make the value larger.
    OpenSslExpect(length <= 32, "invalid padding length");

    auto xComponent = shared_BN::make();
    auto yComponent = shared_BN::make();

    auto* ecKey = EVP_PKEY_get0_EC_KEY(keyPair.removeConst());
    OpenSslExpect(ecKey!=nullptr, "could not extract elliptic curve key from key pair");

    auto* publicKey = EC_KEY_get0_public_key(ecKey);
    OpenSslExpect(publicKey!=nullptr, "could not extract public key from elliptic curve key");

    auto* group = EC_KEY_get0_group(ecKey);
    OpenSslExpect(group!=nullptr, "could not get group from elliptic curve key");

    const int result = EC_POINT_get_affine_coordinates(group, publicKey, xComponent, yComponent, nullptr);
    OpenSslExpect(result==1, "could not get x and y components from elliptic curve public key");

    std::string x (length, 'X');
    int paddedSize = BN_bn2binpad(xComponent, reinterpret_cast<unsigned char*>(x.data()), static_cast<int>(x.size()));
    OpenSslExpect(paddedSize==static_cast<int>(length) && static_cast<size_t>(paddedSize)==length, "could not extract padded x component");

    std::string y (length, 'X');
    paddedSize = BN_bn2binpad(yComponent, reinterpret_cast<unsigned char*>(y.data()), static_cast<int>(y.size()));
    OpenSslExpect(paddedSize==static_cast<int>(length) && static_cast<size_t>(paddedSize)==length, "could not extract padded y component");

    return {std::move(x),std::move(y)};
}


shared_EVP_PKEY EllipticCurveUtils::pemToPrivatePublicKeyPair (const SafeString& pemString)
{
    auto mem = shared_BIO::make();
    const int written = BIO_write(mem, static_cast<const char*>(pemString), gsl::narrow<int>(pemString.size()));
    OpenSslExpect(written >= 0, "reading pem private key failed");
    OpenSslExpect(static_cast<size_t>(written) == pemString.size(), "attempt to write bytes has failed");

    auto privateKey = shared_EVP_PKEY::make();
    auto p = PEM_read_bio_PrivateKey(mem, privateKey.getP(), nullptr, nullptr);
    if (p == nullptr)
        return shared_EVP_PKEY();
    else
        return privateKey;
}

shared_EVP_PKEY EllipticCurveUtils::pemToPublicKey(const SafeString& pem)
{
    // Create public key from string.
    auto mem = shared_BIO::make();
    OpenSslExpect(mem != nullptr, "Failed while allocating structure");
    BIO_puts(mem, pem);
    auto publicKey = shared_EVP_PKEY::make();
    OpenSslExpect(publicKey != nullptr, "Can't create public key structure");
    OpenSslExpect(PEM_read_bio_PUBKEY(mem, publicKey.getP(), nullptr, nullptr) != nullptr, "Can't load pubkey");
    return publicKey;
}


shared_EVP_PKEY EllipticCurveUtils::x962ToPublicKey(const SafeString& derString)
{
    auto mem = shared_BIO::make();
    const int length = BIO_write(mem, static_cast<const char*>(derString), gsl::narrow_cast<int>(derString.size()));
    OpenSslExpect(
        gsl::narrow<int>(derString.size()) == length,
        "deserialization of public key from X9.62 format failed while filling internal buffer");
    // There have been several comments and clarifications from Gematik regarding the binary
    // format for they keys.
    auto key = shared_EC_KEY::make(d2i_EC_PUBKEY_bio(mem, nullptr));
    OpenSslExpect(
        key.isSet(),
        "deserialization of public key from X9.62 format failed while extracting EC key");

    auto pkey = shared_EVP_PKEY::make();
    const int status = EVP_PKEY_set1_EC_KEY(pkey, key);
    OpenSslExpect(
        status==1,
        "deserialization of public key from X9.62 format failed");
    return pkey;
}


util::Buffer EllipticCurveUtils::convertSignatureFormat(const util::Buffer& signature,
                                                        SignatureFormat fromFormat,
                                                        SignatureFormat toFormat)
{
    if (fromFormat == toFormat)
    {
        throw std::logic_error{"Source and destination signature formats cannot be the same"};
    }

    return serializeSignature(unserializeSignature(signature, fromFormat), toFormat);
}


std::string EllipticCurveUtils::publicKeyToPem (const shared_EVP_PKEY& publicKey)
{
    auto memory = shared_BIO::make();
    const int status = PEM_write_bio_PUBKEY(memory, publicKey.removeConst());
    OpenSslExpect(status == 1, "can not convert public key to PEM string");

    char* data = nullptr;
    const std::size_t length = BIO_get_mem_data(memory, &data);
    OpenSslExpect(data != nullptr && length != 0, "can not get data from bio");

    return std::string(data, length);
}


std::string EllipticCurveUtils::publicKeyToX962Der (const shared_EVP_PKEY& publicKey)
{
    Expect(publicKey.isSet(), "the key is missing");

    shared_EC_KEY ecKey = shared_EC_KEY::make(EVP_PKEY_get1_EC_KEY(publicKey.removeConst()));
    Expect(ecKey.isSet(), "serialization of public key to X9.62 format failed while extracting EC key");
    shared_BIO out = shared_BIO::make();
    Expect(out.isSet(), "can not allocate memory for BIO");

    const int status = i2d_EC_PUBKEY_bio(out, ecKey);
    Expect(status==1, "serialization of public key to X9.62 format failed");
    auto unused = BIO_flush(out);
    (void)unused;

    return bioToString(out);
}


std::string EllipticCurveUtils::privateKeyToPem (const shared_EVP_PKEY& privateKey)
{
    auto memory = shared_BIO::make();
    const int status = PEM_write_bio_PrivateKey(memory, privateKey.removeConst(), nullptr, nullptr, 0, nullptr, nullptr);
    OpenSslExpect(status == 1, "can not convert private key to PEM string");

    char* data = nullptr;
    const std::size_t length = BIO_get_mem_data(memory, &data);
    OpenSslExpect(data != nullptr && length != 0, "can not get data from bio");

    return std::string(data, length);
}
