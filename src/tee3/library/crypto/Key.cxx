/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/Key.hxx"
#include "library/log/Logging.hxx"
#include "library/util/Assert.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

#define throwIfNot(expression, message) AssertOpenSsl(expression) << message


std::string Key::privateKeyToPemString(const shared_EVP_PKEY& key)
{
    throwIfNot(key.isSet(), "the key must be provided");

    auto mem = shared_BIO::make();
    const int status =
        PEM_write_bio_PrivateKey(mem.get(), key, nullptr, nullptr, 0, nullptr, nullptr);
    throwIfNot(status == 1, "serialization of private key to PEM format failed");
    return bioToString(mem.get());
}


shared_EVP_PKEY Key::privateKeyFromPemString(const std::string& pemString)
{
    auto mem = shared_BIO::make();
    const int written = BIO_write(mem, pemString.data(), gsl::narrow<int>(pemString.size()));
    throwIfNot(written == gsl::narrow<int>(pemString.size()), "attempt to write bytes has failed");

    auto privateKey = shared_EVP_PKEY::make();
    auto* p = PEM_read_bio_PrivateKey(mem, privateKey.getP(), nullptr, nullptr);
    if (p == nullptr)
        return shared_EVP_PKEY();
    else
        return privateKey;
}


shared_EVP_PKEY Key::privateKeyFromDer(const std::string& derString)
{
    // There have been several comments and clarifications from Gematik regarding the binary
    // format for they keys.
    const uint8_t* p = reinterpret_cast<const uint8_t*>(derString.data());
    auto* privateKey =
        d2i_PrivateKey(EVP_PKEY_EC, nullptr, &p, gsl::narrow<long>(derString.size()));

    auto pkey = shared_EVP_PKEY::make(privateKey);
    Assert(pkey != nullptr);
    return pkey;
}


std::string Key::publicKeyToPemString(const shared_EVP_PKEY& key)
{
    throwIfNot(key.isSet(), "the key must be provided");

    auto mem = shared_BIO::make();
    const int status = PEM_write_bio_PUBKEY(mem.get(), key);
    throwIfNot(status == 1, "serialization of public key to PEM format failed");
    return bioToString(mem.get());
}


shared_EVP_PKEY Key::publicKeyFromPemString(const std::string& pemString)
{
    auto mem = shared_BIO::make();
    const int written = BIO_write(mem, pemString.data(), gsl::narrow<int>(pemString.size()));
    throwIfNot(written == gsl::narrow<int>(pemString.size()), "attempt to write bytes has failed");
    return shared_EVP_PKEY::make(PEM_read_bio_PUBKEY(mem, nullptr, nullptr, nullptr));
}


// GEMREQ-start A_16901
std::string Key::publicKeyToX962DerString(const shared_EVP_PKEY& keyPair)
{
    throwIfNot(keyPair.isSet(), "the key must be provided");

    const EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(keyPair);
    throwIfNot(
        ecKey != nullptr,
        "serialization of public key to X9.62 format failed while extracting EC key");
    shared_BIO out = shared_BIO::make();
    const int status = i2d_EC_PUBKEY_bio(out, ecKey);
    throwIfNot(status == 1, "serialization of public key to X9.62 format failed");
    auto unused = BIO_flush(out);
    (void) unused;

    return bioToString(out);
}
// GEMREQ-end A_16901


shared_EVP_PKEY Key::publicKeyFromX962Der(const std::string& derString)
{
    auto mem = shared_BIO::make();
    const int length = BIO_write(mem, derString.data(), gsl::narrow<int>(derString.size()));
    throwIfNot(
        static_cast<int>(derString.size()) == length,
        "deserialization of public key from X9.62 format failed while filling internal buffer");
    // There have been several comments and clarifications from Gematik regarding the binary
    // format for they keys.
    auto key = shared_EC_KEY::make(d2i_EC_PUBKEY_bio(mem, nullptr));
    throwIfNot(
        key.isSet(),
        "deserialization of public key from X9.62 format failed while extracting EC key");

    auto pkey = shared_EVP_PKEY::make();
    const int status = EVP_PKEY_set1_EC_KEY(pkey, key);
    throwIfNot(status == 1, "deserialization of public key from X9.62 format failed");
    return pkey;
}


Key::PaddedComponents Key::getPaddedXYComponents(const EVP_PKEY& keyPair, const size_t length)
{
    // Limit the padding length so that conversion from size_t to int does not make a problem.
    // If 32 is not sufficient for a future use case, make the value larger.
    AssertOpenSsl(length <= 32) << "invalid padding length";

    auto xComponent = shared_BN::make();
    auto yComponent = shared_BN::make();

    const EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(&keyPair);
    AssertOpenSsl(ecKey != nullptr) << "could not extract elliptic curve key from key pair";

    const EC_POINT* publicKey = EC_KEY_get0_public_key(ecKey);
    AssertOpenSsl(publicKey != nullptr) << "could not extract public key from elliptic curve key";

    const EC_GROUP* group = EC_KEY_get0_group(ecKey);
    AssertOpenSsl(group != nullptr) << "could not get group from elliptic curve key";

    const int result =
        EC_POINT_get_affine_coordinates(group, publicKey, xComponent, yComponent, nullptr);
    AssertOpenSsl(result == 1) << "could not get x and y components from elliptic curve public key";

    std::string x(length, 'X');
    int paddedSize = BN_bn2binpad(
        xComponent, reinterpret_cast<unsigned char*>(x.data()), static_cast<int>(x.size()));
    AssertOpenSsl(
        paddedSize == static_cast<int>(length) && static_cast<size_t>(paddedSize) == length)
        << "could not extract padded x component";

    std::string y(length, 'X');
    paddedSize = BN_bn2binpad(
        yComponent, reinterpret_cast<unsigned char*>(y.data()), static_cast<int>(y.size()));
    AssertOpenSsl(
        paddedSize == static_cast<int>(length) && static_cast<size_t>(paddedSize) == length)
        << "could not extract padded y component";

    return {std::move(x), std::move(y)};
}


shared_EVP_PKEY Key::createPublicKeyBin(
    const std::string_view& xCoordinateBin,
    const std::string_view& yCoordinateBin,
    const int curveId)
{
    AssertOpenSsl(xCoordinateBin.size() <= 32)
        << "x coordinate too long, " << logging::confidential(std::to_string(xCoordinateBin.size()))
        << " but must not be longer than 32";
    AssertOpenSsl(yCoordinateBin.size() <= 32)
        << "y coordinate too long, " << logging::confidential(std::to_string(yCoordinateBin.size()))
        << " but must not be longer than 32";

    auto x = shared_BN::make();
    auto* result = BN_bin2bn(
        reinterpret_cast<const unsigned char*>(xCoordinateBin.data()),
        static_cast<int>(xCoordinateBin.size()),
        x);
    AssertOpenSsl(result == x.get()) << "conversion of x component failed";

    auto y = shared_BN::make();
    result = BN_bin2bn(
        reinterpret_cast<const unsigned char*>(yCoordinateBin.data()),
        static_cast<int>(yCoordinateBin.size()),
        y);
    AssertOpenSsl(result == y.get()) << "conversion of y component failed";

    return createPublicKeyBN(x, y, curveId);
}


shared_EVP_PKEY Key::createPublicKeyBN(
    const shared_BN& xCoordinate,
    const shared_BN& yCoordinate,
    const int curveId)
{
    auto key = shared_EC_KEY::make(EC_KEY_new_by_curve_name(curveId));
    AssertOpenSsl(key.isSet()) << "could not create new EC key on elliptic curve";
    EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
    int status = EC_KEY_generate_key(key);
    AssertOpenSsl(status == 1) << "generating EVP key from EC key failed";

    const auto* group = EC_KEY_get0_group(key);
    AssertOpenSsl(group != nullptr) << "could not get group from elliptic curve key";

    auto publicKey = shared_EC_POINT::make(EC_POINT_new(group));
    int result =
        EC_POINT_set_affine_coordinates(group, publicKey, xCoordinate, yCoordinate, nullptr);
    AssertOpenSsl(result == 1) << "could not create public key from x,y components";

    result = EC_KEY_set_public_key(key, publicKey);
    AssertOpenSsl(result == 1) << "could not create ec key public key";

    auto pkey = shared_EVP_PKEY::make();

    status = EVP_PKEY_set1_EC_KEY(pkey, key);
    AssertOpenSsl(status == 1) << "could not convert key into EC key";

    return pkey;
}


size_t Key::getAlgorithmBitCount(const EVP_PKEY& keyPair)
{
    return gsl::narrow<size_t>(EVP_PKEY_get_bits(&keyPair));
}

} // namespace epa
