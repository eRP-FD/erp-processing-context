/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/crypto/EllipticCurve.hxx"
#include "library/crypto/EllipticCurveUtils.hxx"
#include "library/crypto/Key.hxx"
#include "library/crypto/tee3/Kyber768.hxx"
#include "library/util/Assert.hxx"
#include "library/util/cbor/CborSerializer.hxx"

#include <regex>

#include "shared/util/Base64.hxx"

namespace epa
{

Tee3Protocol::EcdhPublicKey Tee3Protocol::EcdhPublicKey::fromEvpPkey(const shared_EVP_PKEY& evpPkey)
{
    Assert(evpPkey.isSet());

    Tee3Protocol::EcdhPublicKey ecdhKey;
    const auto xy = Key::getPaddedXYComponents(*evpPkey, 32);
    Assert(xy.x.length() == 32);
    Assert(xy.y.length() == 32);

    return Tee3Protocol::EcdhPublicKey{.x = BinaryBuffer(xy.x), .y = BinaryBuffer(xy.y)};
}


shared_EVP_PKEY Tee3Protocol::EcdhPublicKey::toEvpPkey() const
{
    return Key::createPublicKeyBin(
        binaryBufferToString(x), binaryBufferToString(y), EllipticCurveUtils::P256);
}


BinaryBuffer Tee3Protocol::EcdhPublicKey::serialize() const
{
    return CborSerializer::serialize(*this);
}


Tee3Protocol::PrivateKeys Tee3Protocol::Keypairs::privateKeys() const
{
    return PrivateKeys{
        .ecdhPrivateKey = ecdh.privateKey, .kyber768PrivateKey = kyber768.privateKey};
}


// The length should be validated in Tee3Protocol::VauCid::verify()
const boost::regex Tee3Protocol::VauCid::mVauCidRegex = boost::regex("^/[a-zA-Z0-9/-]*$");


Tee3Protocol::VauCid::VauCid()
  : mValue()
{
}


Tee3Protocol::VauCid::VauCid(const std::string& value)
  : mValue(value)
{
}


Tee3Protocol::VauCid::VauCid(std::string&& value)
  : mValue(std::move(value))
{
}


Tee3Protocol::VauCid& Tee3Protocol::VauCid::operator=(const VauCid& other)
{
    if (this != &other)
        mValue = other.mValue;
    return *this;
}


Tee3Protocol::VauCid& Tee3Protocol::VauCid::operator=(VauCid&& other) noexcept
{
    if (this != &other)
        mValue = std::move(other.mValue);
    return *this;
}


bool Tee3Protocol::VauCid::empty() const
{
    return mValue.empty();
}


const std::string& Tee3Protocol::VauCid::toString() const
{
    verify();
    return mValue;
}


void Tee3Protocol::VauCid::verify() const
{
    // Currently the cluster domain name is used to identify the cluster,
    // as result the VAU-CID can be easily bigger than 200 characters.
    // TODO: reactivate when we have a solution for the long URLs that are caused
    // by including cluster and node names.
    // Assert(mValue.size() <= 200);                 // A_24608, A_24622
    LOG(DEBUG1) << "VAU-CID is " << mValue;
    Assert(! mValue.empty()) // A_24622
        << "VAU-CID is empty" << assertion::exceptionType<TeeError>(TeeError::Code::DecodingError);
    Assert(mValue[0] == '/') // A_24622
        << "VAU-CID '" << mValue << "' does not start with '/'"
        << assertion::exceptionType<TeeError>(TeeError::Code::DecodingError);
    Assert(boost::regex_match(mValue, mVauCidRegex))
        << "VAU-CID '" << mValue << "' does match regexp " << mVauCidRegex
        << assertion::exceptionType<TeeError>(TeeError::Code::DecodingError);
}


Tee3Protocol::Keypairs Tee3Protocol::Keypairs::generate()
{
    // Note that prime256v1 == secp256r1 == P-256
    const auto eccdhKeyPair = EllipticCurve::Prime256v1->createKeyPair();

    auto [Kyber768PublicKey, Kyber768PrivateKey] = Kyber768::createKeyPair();

    // clang-format off
    return Tee3Protocol::Keypairs{
        .ecdh = {
            .publicKey = Tee3Protocol::EcdhPublicKey::fromEvpPkey(eccdhKeyPair),
            .privateKey = SensitiveDataGuard(Key::privateKeyToPemString(eccdhKeyPair))
        },
        .kyber768 = {
            .publicKey = std::move(Kyber768PublicKey),
            .privateKey = std::move(Kyber768PrivateKey)
        }
    };
    // clang-format on
}

} // namespace epa