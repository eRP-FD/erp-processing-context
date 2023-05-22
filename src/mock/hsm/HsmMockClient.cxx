/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/hsm/HsmMockClient.hxx"

#include "erp/crypto/EllipticCurve.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Random.hxx"
#include "erp/util/String.hxx"

#include "erp/util/TLog.hxx"

#include <sstream>


namespace
{
    constexpr std::string_view defaultTeeBlob ("this is not a real TEE blob");
}

namespace {
    std::string stringFromVector (const std::vector<uint8_t>& data)
    {
        return std::string(
            reinterpret_cast<const char*>(data.data()),
            data.size());
    }
}


HsmMockClient::HsmMockClient (void)
{
    TVLOG(0) << "creating mock HSM client";
}

::Nonce HsmMockClient::getNonce([[maybe_unused]] const ::HsmRawSession& session, [[maybe_unused]] uint32_t input)
{
    return {};
}

ErpBlob HsmMockClient::generatePseudonameKey(const ::HsmRawSession& session, uint32_t input)
{
    (void)session;
    ErpVector v{Random::randomBinaryData(32)};
    ErpBlob b;
    b.generation = input;
    b.data = SafeString(v.data(), v.size());
    return b;
}

ErpArray<Aes256Length> HsmMockClient::unwrapPseudonameKey(const HsmRawSession& session, UnwrapHashKeyInput&& input)
{
    return unwrapHashKey(session, std::move(input));
}

ErpBlob HsmMockClient::getTeeToken(
    const HsmRawSession&,
    TeeTokenRequestInput&& input)
{
    (void)input;

    std::vector<uint8_t> tokenData;
    tokenData.resize(defaultTeeBlob.size());
    std::copy(defaultTeeBlob.begin(), defaultTeeBlob.end(), tokenData.data());
    // Remember the tee token.
    // Subsequent calls to other methods of this class are expected to authenticate via this token.
    mTeeToken = ErpBlob(std::move(tokenData), 0);
    return mTeeToken;
}


DeriveKeyOutput HsmMockClient::deriveTaskKey(
    const HsmRawSession&,
    DeriveKeyInput&& input)
{
    verifyTeeToken(input.teeToken);

    return derivePersistenceKey(std::move(input), ::BlobType::TaskKeyDerivation);
}


DeriveKeyOutput HsmMockClient::deriveAuditKey(
    const HsmRawSession&,
    DeriveKeyInput&& input)
{
    verifyTeeToken(input.teeToken);

    return derivePersistenceKey(std::move(input), ::BlobType::AuditLogKeyDerivation);
}


DeriveKeyOutput HsmMockClient::deriveCommsKey(
    const HsmRawSession&,
    DeriveKeyInput&& input)
{
    verifyTeeToken(input.teeToken);

    return derivePersistenceKey(std::move(input), ::BlobType::CommunicationKeyDerivation);
}

::DeriveKeyOutput HsmMockClient::deriveChargeItemKey([[maybe_unused]] const::HsmRawSession& session, ::DeriveKeyInput&& input)
{
    verifyTeeToken(input.teeToken);

    return derivePersistenceKey(::std::move(input), ::BlobType::ChargeItemKeyDerivation);
}

ErpArray<Aes128Length> HsmMockClient::doVauEcies128(
    const HsmRawSession&,
    DoVAUECIESInput&& input)
{
    verifyTeeToken(input.teeToken);

    DiffieHellman dh;
    dh.setPrivatePublicKey(
        EllipticCurveUtils::pemToPrivatePublicKeyPair(input.eciesKeyPair.data));
    dh.setPeerPublicKey(
        EllipticCurveUtils::x962ToPublicKey(
            SafeString(
                stringFromVector(input.clientPublicKey))));
    // Use BrainpoolP256r1 for the ecdh according to https://dth01.ibmgcloud.net/confluence/pages/viewpage.action?pageId=74023036&preview=/74023036/81467265/LG-051%20Produkt-Kryptokonzept%20v1.0.pdf
    const auto sharedKey = dh.createSharedKey();

    SafeString symmetricKey{DiffieHellman::hkdf(sharedKey, keyDerivationInfo, Aes128Length)};
    Expect(symmetricKey.size()==Aes128Length, "dh created key of wrong size");

    ErpArray<Aes128Length> output;
    const std::string_view sv = symmetricKey;
    std::copy(sv.begin(), sv.end(), output.data());
    return output;
}

shared_EVP_PKEY HsmMockClient::getEcPublicKey(const HsmRawSession&, ErpBlob&& ecKeyPair)
{
    return EllipticCurveUtils::pemToPrivatePublicKeyPair(ecKeyPair.data);
}

SafeString HsmMockClient::getVauSigPrivateKey (
    const HsmRawSession&,
    GetVauSigPrivateKeyInput&& input)
{
    verifyTeeToken(input.teeToken);
    return SafeString(std::move(input.vauSigPrivateKey.data));
}


ErpVector HsmMockClient::getRndBytes(
    const HsmRawSession&,
    size_t input)
{
    // In order to keep tests repeatable we don't return true or pseudo random data but a fixed string,
    // repeated as often as necessary.

    ErpVector result;
    result.resize(input);
    size_t remaining = input;
    auto p = result.begin();
    while(remaining > 0)
    {
        const size_t count = std::min(remaining, defaultRandomData.size());
        std::copy(defaultRandomData.begin(), defaultRandomData.begin()+count, p);
        p += gsl::narrow<ptrdiff_t>(count);
        remaining -= count;
    }
    return result;
}


ErpArray<Aes256Length> HsmMockClient::unwrapHashKey(
    const HsmRawSession&,
    UnwrapHashKeyInput&& input)
{
    verifyTeeToken(input.teeToken);

    Expect(input.key.data.size() == Aes256Length, "key blob has the wrong size");
    ErpArray<Aes256Length> result;
    const unsigned char* begin = input.key.data;
    std::copy(begin, begin + input.key.data.size(), result.data());
    return result;
}

::ParsedQuote HsmMockClient::parseQuote([[maybe_unused]] const ::ErpVector& quote) const
{
    return {};
}

DeriveKeyOutput HsmMockClient::derivePersistenceKey(DeriveKeyInput&& input, ::BlobType expectedBlobType)
{
    if (input.initialDerivation)
    {
        // Add some "random" bytes to input.derivationData.
        // The production client will add a different number of bytes. This is so that the caller
        // does not hard code any expectation that might be not true in the next release of the hsm(client).
        input.derivationData.reserve(input.derivationData.size() + defaultRandomData.size());
        std::copy(defaultRandomData.begin(), defaultRandomData.end(), std::back_inserter(input.derivationData));
    }

    input.derivationData.push_back(static_cast<decltype(input.derivationData)::value_type>(expectedBlobType));

    const std::string symmetricKey = DiffieHellman::hkdf(
        input.derivationKey.data,
        stringFromVector(input.derivationData),
        Aes256Length);
    Expect(symmetricKey.size() == Aes256Length, "hkdf failed");

    DeriveKeyOutput output;
    if (input.initialDerivation)
    {
        output.optionalData = OptionalDeriveKeyData{
            ErpVector::create(defaultRandomData),
            input.blobId
        };
    }
    else
    {
        // Signal caller that salt and blob generation where not modified.
        output.optionalData = {};
    }

    std::copy(symmetricKey.begin(), symmetricKey.end(), output.derivedKey.data());
    return output;
}


void HsmMockClient::verifyTeeToken (const ErpBlob& teeToken) const
{
    Expect(mTeeToken.data.size() == teeToken.data.size(), "invalid tee token size");
    Expect(mTeeToken.data == teeToken.data, "invalid tee token data");
    Expect(mTeeToken.generation == teeToken.generation, "invalid tee token generation");
}

ErpBlob HsmMockClient::wrapRawPayload(const HsmRawSession&, WrapRawPayloadInput&& input)
{
    verifyTeeToken(input.teeToken);
    return ErpBlob{std::move(input.rawPayload), input.generation};
}

ErpVector HsmMockClient::unwrapRawPayload(const HsmRawSession&, UnwrapRawPayloadInput&& input)
{
    verifyTeeToken(input.teeToken);
    return ErpVector::create(input.wrappedRawPayload.data);
}

void HsmMockClient::reconnect (HsmRawSession&)
{
    // Nothing to do for the mock client.
}
