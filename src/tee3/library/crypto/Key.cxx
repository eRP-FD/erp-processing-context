/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/Key.hxx"
#include "library/log/Logging.hxx"
#include "library/util/Assert.hxx"
#include "library/util/BinaryBuffer.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include "fhirtools/util/Gsl.hxx"

#include <openssl/param_build.h>

namespace epa
{

namespace
{
constexpr std::size_t componentSize = 32;
}

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


Key::PaddedComponents Key::getPaddedXYComponents(const EVP_PKEY& keyPair, const size_t length)
{
    // Limit the padding length so that conversion from size_t to int does not make a problem.
    // If 32 is not sufficient for a future use case, make the value larger.
    AssertOpenSsl(length <= 32) << "invalid padding length";

    BIGNUM *bnX = nullptr;
    BIGNUM *bnY = nullptr;

    auto result = EVP_PKEY_get_bn_param(&keyPair, OSSL_PKEY_PARAM_EC_PUB_X, &bnX);
    auto xComponent = shared_BN::make(bnX);
    AssertOpenSsl(result == 1) << "EVP_PKEY_get_bn_param failed";
    result = EVP_PKEY_get_bn_param(&keyPair, OSSL_PKEY_PARAM_EC_PUB_Y, &bnY);
    AssertOpenSsl(result == 1) << "EVP_PKEY_get_bn_param failed";
    auto yComponent = shared_BN::make(bnY);

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
    BinaryBuffer pubkeyUncompressed(1 + (2 * componentSize));
    auto pubkeySpan = std::span{pubkeyUncompressed.data(), pubkeyUncompressed.size()};

    pubkeySpan[0] = 0x4;
    int paddedSize =
        BN_bn2binpad(xCoordinate, pubkeySpan.subspan(1, 1 + componentSize).data(), componentSize);
    AssertOpenSsl(paddedSize == static_cast<int>(componentSize))
        << "could not extract padded x component";
    paddedSize = BN_bn2binpad(
        yCoordinate,
        pubkeySpan.subspan(1 + componentSize, 1 + (2 * componentSize)).data(),
        componentSize);
    AssertOpenSsl(paddedSize == static_cast<int>(componentSize))
        << "could not extract padded y component";

    const std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> pctx{
        EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), &EVP_PKEY_CTX_free};
    throwIfNot(EVP_PKEY_fromdata_init(pctx.get()) == 1, "EVP_PKEY_fromdata_init failed");
    const std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)> paramsBuild{
        OSSL_PARAM_BLD_new(), &OSSL_PARAM_BLD_free};

    OSSL_PARAM_BLD_push_utf8_string(
        paramsBuild.get(), OSSL_PKEY_PARAM_GROUP_NAME, OBJ_nid2sn(curveId), 0);
    OSSL_PARAM_BLD_push_octet_string(
        paramsBuild.get(),
        OSSL_PKEY_PARAM_PUB_KEY,
        pubkeyUncompressed.data(),
        pubkeyUncompressed.size());
    const std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)> params{
        OSSL_PARAM_BLD_to_param(paramsBuild.get()), &OSSL_PARAM_free};
    throwIfNot(params != nullptr, "Failed to generate OSSL_PARAM");

    EVP_PKEY* pkey = nullptr;
    auto result = EVP_PKEY_fromdata(pctx.get(), &pkey, EVP_PKEY_PUBLIC_KEY, params.get());

    throwIfNot(result == 1, "EVP_PKEY_fromdata failed");
    throwIfNot(pkey != nullptr, "Unable to create public key");

    return shared_EVP_PKEY::make(pkey);
}


size_t Key::getAlgorithmBitCount(const EVP_PKEY& keyPair)
{
    return gsl::narrow<size_t>(EVP_PKEY_get_bits(&keyPair));
}

} // namespace epa
