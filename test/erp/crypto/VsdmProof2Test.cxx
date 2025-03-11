/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/VsdmProof.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Buffer.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Random.hxx"

#include <gtest/gtest.h>


TEST(VsdmProof2Test, roundTrip)
{
    VsdmHmacKey keyPackage('A', '1');
    model::Kvnr kvnr{"X123456788"};
    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));
    auto hcv = VsdmProof2::makeHcv("20180912", "Beispielstrasse");

    auto nowEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // adjust the timestamp to reduce the accuracy as it is done
    // when transmitting the data
    nowEpoch >>= 3;
    nowEpoch <<= 3;
    auto inputProof = VsdmProof2::DecryptedProof{
        .revoked = false,
        .hcv = hcv,
        .iat = model::Timestamp(std::chrono::system_clock::time_point{std::chrono::seconds(nowEpoch)}),
        .kvnr = kvnr};
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, inputProof);

    ASSERT_EQ(encryptedProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(encryptedProof.keyVersion(), keyPackage.version());

    auto pzB64 = encryptedProof.serialize();
    ASSERT_EQ(pzB64.size(), 64);

    auto pz = Base64::decodeToString(pzB64);
    auto vsdmProof = VsdmProof2::deserialize(pz);

    ASSERT_EQ(vsdmProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(vsdmProof.keyVersion(), keyPackage.version());

    auto sharedSecret = SafeString{Base64::decode(keyPackage.plainTextKey())};
    auto decryptedProof = vsdmProof.decrypt(sharedSecret);

    ASSERT_EQ(inputProof.iat, decryptedProof.iat);
    ASSERT_EQ(inputProof.revoked, decryptedProof.revoked);
    ASSERT_EQ(inputProof.hcv, decryptedProof.hcv);
    ASSERT_EQ(inputProof.kvnr, decryptedProof.kvnr);
}


TEST(VsdmProof2Test, roundTripRevoked)
{
    VsdmHmacKey keyPackage('A', '1');
    model::Kvnr kvnr{"X123456788"};
    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));
    auto hcv = VsdmProof2::makeHcv("20230123", "Hauptstr");

    auto nowEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // adjust the timestamp to reduce the accuracy as it is done
    // when transmitting the data
    nowEpoch >>= 3;
    nowEpoch <<= 3;
    auto inputProof = VsdmProof2::DecryptedProof{
        .revoked = true,
        .hcv = hcv,
        .iat = model::Timestamp(std::chrono::system_clock::time_point{std::chrono::seconds(nowEpoch)}),
        .kvnr = kvnr};
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, inputProof);

    ASSERT_EQ(encryptedProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(encryptedProof.keyVersion(), keyPackage.version());

    auto pzB64 = encryptedProof.serialize();
    ASSERT_EQ(pzB64.size(), 64);

    auto pz = Base64::decodeToString(pzB64);
    auto vsdmProof = VsdmProof2::deserialize(pz);

    ASSERT_EQ(vsdmProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(vsdmProof.keyVersion(), keyPackage.version());

    auto sharedSecret = SafeString{Base64::decode(keyPackage.plainTextKey())};
    auto decryptedProof = vsdmProof.decrypt(sharedSecret);

    ASSERT_EQ(inputProof.iat, decryptedProof.iat);
    ASSERT_EQ(inputProof.revoked, decryptedProof.revoked);
    ASSERT_EQ(inputProof.hcv, decryptedProof.hcv);
    ASSERT_EQ(inputProof.kvnr, decryptedProof.kvnr);
}


TEST(VsdmProof2Test, refData)
{
    model::Kvnr kvnr{"A123456789"};
    VsdmHmacKey keyPackage('X', '2');
    util::Buffer key =
        util::stringToBuffer(ByteHelper::fromHex("0000000000000000000000000000000000000000000000000000000000000001"));
    keyPackage.setPlainTextKey(Base64::encode(key));

    auto refIat = model::Timestamp::fromXsDateTime("2025-01-20T13:46:08.000+00:00");

    auto pz = Base64::decodeToString("3jUSmHW5Jzobg0FO8gJ5YYeDASjwPhmCNgm/bkikm/BkCVztHCtSguADsExM73Q=");
    auto encryptedProof = VsdmProof2::deserialize(pz);
    ASSERT_EQ(encryptedProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(encryptedProof.keyVersion(), keyPackage.version());

    auto sharedSecret = SafeString{Base64::decode(keyPackage.plainTextKey())};
    auto decryptedProof = encryptedProof.decrypt(sharedSecret);
    auto hcv = VsdmProof2::makeHcv("20180912", "Beispielstrasse");
    ASSERT_EQ(decryptedProof.iat, refIat);
    ASSERT_EQ(decryptedProof.revoked, false);
    ASSERT_EQ(decryptedProof.hcv, hcv);
    ASSERT_EQ(decryptedProof.kvnr, kvnr);
}


TEST(VsdmProof2Test, invalidVersion)
{
    VsdmHmacKey keyPackage('A', '1');
    model::Kvnr kvnr{"X123456788"};
    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));
    auto hcv = VsdmProof2::makeHcv("20240101", "Fleet Street");
    auto inputProof = VsdmProof2::DecryptedProof{
        .revoked = false,
        .hcv = hcv,
        .iat = model::Timestamp::now(),
        .kvnr = kvnr};
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, inputProof);

    ASSERT_EQ(encryptedProof.keyOperatorId(), keyPackage.operatorId());
    ASSERT_EQ(encryptedProof.keyVersion(), keyPackage.version());

    auto pzB64 = encryptedProof.serialize();
    ASSERT_EQ(pzB64.size(), 64);

    auto pz = Base64::decodeToString(pzB64);
    pz[0] = gsl::narrow<char>(static_cast<uint8_t>(pz[0]) & 127);
    EXPECT_ANY_THROW(VsdmProof2::deserialize(pz));
}
