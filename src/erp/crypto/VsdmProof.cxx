/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/VsdmProof.hxx"

#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/DiffieHellman.hxx"
#include "shared/crypto/SecureRandomGenerator.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/Random.hxx"

#include <gsl/gsl-lite.hpp>

char VsdmProof2::keyOperatorId() const
{
    return mKeyOperatorId;
}

char VsdmProof2::keyVersion() const
{
    return mKeyVersion;
}

// GEMREQ-start A_27279#deserialize
VsdmProof2 VsdmProof2::deserialize(std::string_view validationData)
{
    // A_27279: step 2
    ErpExpect(validationData.size() == binarySize, HttpStatus::Forbidden, "Invalid size of Prüfziffer");
    VsdmProof2 proof;
    const auto field1 = static_cast<uint8_t>(validationData[0]);
    // A_27279: step 3
    Expect(field1 > 128, "Unexpected version field value");
    proof.mKeyVersion = gsl::narrow<char>((field1 & 3) + '0');
    Expect(proof.mKeyVersion < '4' && proof.mKeyVersion >= '0', "VSDM Key Version in invalid range");
    proof.mKeyOperatorId = gsl::narrow<char>(((field1 & 127) >> 2) + 'A'); // NOLINT
    Expect(proof.mKeyOperatorId <= 'Z' && proof.mKeyOperatorId >= 'A', "VSDM Operator Id in invalid range");

    proof.mIv = validationData.substr(1, AesGcm128::IvLength);
    proof.mCiphertext = validationData.substr(1 + AesGcm128::IvLength, encryptedContentLength);
    proof.mAuthenticationTag =
        validationData.substr(1 + AesGcm128::IvLength + encryptedContentLength, AesGcm128::AuthenticationTagLength);
    return proof;
}
// GEMREQ-end A_27279#deserialize

// GEMREQ-start A_27279#decrypt
VsdmProof2::DecryptedProof VsdmProof2::decrypt(const SafeString& sharedSecret) const
{
    auto aesKey = SafeString{DiffieHellman::hkdf(sharedSecret, derivationKey, AesGcm128::KeyLength)};
    auto plainText = AesGcm128::decrypt(mCiphertext, aesKey, mIv, mAuthenticationTag);
    Expect(plainText.size() == encryptedContentLength, "Unexpected size of decrypted content");
    auto plainTextView = static_cast<std::string_view>(plainText);
    bool revoked = (static_cast<uint8_t>(plainTextView[0]) & 128) != 0; // NOLINT
    std::string hcv;
    hcv.resize(hcvSize);

    std::copy_n(plainTextView.begin(), hcvSize, hcv.begin());
    hcv[0] = static_cast<char>(static_cast<uint8_t>(hcv[0]) & 127); // NOLINT
    uint32_t iat_raw = static_cast<uint32_t>(((static_cast<uint8_t>(plainTextView[5]) << 16) |// NOLINT
                                              (static_cast<uint8_t>(plainTextView[6]) << 8) | // NOLINT
                                              static_cast<uint8_t>(plainTextView[7]))         // NOLINT
                                             << 3);
    auto iat = std::chrono::seconds(iat_raw) + iatEpochOffset;
    std::string kvnr;
    kvnr.resize(10); // NOLINT
    std::copy_n(plainTextView.begin() + 8, 10, kvnr.begin()); // NOLINT
    return DecryptedProof{
        .revoked = revoked,
        .hcv = std::move(hcv),
        .iat = model::Timestamp(std::chrono::system_clock::time_point{iat}),
        .kvnr = model::Kvnr(kvnr)
    };
}
// GEMREQ-end A_27279#decrypt

std::string VsdmProof2::makeHcv(std::string_view insuranceStart, std::string_view streetAddress)
{
    auto digest = Hash::sha256(std::string{insuranceStart}.append(streetAddress));

    std::string output;
    output.resize(hcvSize);
    std::copy_n(digest.begin(), hcvSize, output.begin());
    *output.begin() = static_cast<char>(static_cast<uint8_t>(digest[0]) & 127); // NOLINT
    return output;
}

VsdmProof2 VsdmProof2::encrypt(const VsdmHmacKey& vsdmKey, const DecryptedProof& proofData)
{
    VsdmProof2 encryptedProof;
    Expect(proofData.hcv.size() == 5, "Invalid size for hcv");
    Expect(vsdmKey.version() < '4' && vsdmKey.version() >= '0', "VSDM Key Version in invalid range");
    Expect(vsdmKey.operatorId() <= 'Z' && vsdmKey.operatorId() >= 'A', "VSDM Operator ID in invalid range");
    encryptedProof.mKeyVersion = vsdmKey.version();
    encryptedProof.mKeyOperatorId = vsdmKey.operatorId();

    std::string cleartext;
    cleartext.reserve(encryptedContentLength);
    {
        uint8_t S = proofData.revoked ? 128 : 0;// NOLINT
        cleartext.append(proofData.hcv);
        *cleartext.begin() = static_cast<char>(*cleartext.begin() | S);

        const auto secondsSinceEpoch =
            std::chrono::duration_cast<std::chrono::seconds>(proofData.iat.toChronoTimePoint().time_since_epoch());
        auto iatTimestamp = static_cast<uint32_t>((secondsSinceEpoch - iatEpochOffset).count() >> 3);
        Expect(iatTimestamp < (2 << 24), "timestamp too large");

        cleartext.push_back(static_cast<char>((iatTimestamp >> 16) & 0xFF));// NOLINT
        cleartext.push_back(static_cast<char>((iatTimestamp >> 8) & 0xFF)); // NOLINT
        cleartext.push_back(static_cast<char>((iatTimestamp) & 0xFF));      // NOLINT
        cleartext.append(proofData.kvnr.id());
    }

    encryptedProof.mIv = static_cast<std::string_view>(SecureRandomGenerator::generate(AesGcm128::IvLength));

    const auto sharedSecret = SafeString{Base64::decode(vsdmKey.plainTextKey())};

    auto aesKey = SafeString{DiffieHellman::hkdf(sharedSecret, derivationKey, AesGcm128::KeyLength)};
    auto aesResult = AesGcm128::encrypt(cleartext, aesKey, encryptedProof.mIv);

    encryptedProof.mCiphertext = std::move(aesResult.ciphertext);
    encryptedProof.mAuthenticationTag = std::move(aesResult.authenticationTag);

    return encryptedProof;
}


std::string VsdmProof2::serialize()
{
    std::string output;
    output.reserve(binarySize);
    constexpr uint8_t proofVersionBit = 128;
    const auto keyVersionByte = static_cast<uint8_t>(mKeyVersion - '0');
    const auto operatorIdByte = static_cast<uint8_t>((mKeyOperatorId - 'A') << 2);
    output.push_back(static_cast<char>(proofVersionBit + operatorIdByte + keyVersionByte));

    output.append(mIv);
    output.append(mCiphertext);
    output.append(mAuthenticationTag);

    return Base64::encode(output);
}
