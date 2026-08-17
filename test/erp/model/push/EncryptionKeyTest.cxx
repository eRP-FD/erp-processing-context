/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/push/EncryptionKey.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"

#include <gtest/gtest.h>
#include <string>


class EncryptionKeyTest : public ::testing::Test
{
protected:
    model::EncryptionKey defaultKey()
    {
        // data from https://github.com/gematik/gem-push-notifications-concept/blob/1.1/docs/concept.adoc#beispiel-f%C3%BCr-einen-austausch-im-oktober-2023
        SafeString initialSharedSecret(
            ByteHelper::fromHex("f2ca1bb6c7e907d06dafe4687e579fce76b37e4e93b7605022da52e6ccc26fd2"));
        std::string initialMonth("2023-10");
        return model::EncryptionKey::deriveMonthlyKey(initialSharedSecret, initialMonth);
    }
};

TEST_F(EncryptionKeyTest, serialization)
{
    const model::EncryptionKey ec{SafeString{"shared,secret,111111111111111111"},
                                  SafeString{"\tmessage, key 111111111111111111"}, "2026-05"};
    auto serialized = ec.serializeToCsv("keyIdentifier"); // The key identifier is kept in the internal csv member attribute.
    std::optional<model::EncryptionKey> ec2;
    ASSERT_NO_FATAL_FAILURE(ec2.emplace(model::EncryptionKey::deserializeCsv(serialized)));
    ASSERT_TRUE(ec2.has_value());
    EXPECT_STREQ(ec.sharedSecret().c_str(), ec2->sharedSecret().c_str());
    EXPECT_STREQ(ec.messageKey().c_str(), ec2->messageKey().c_str());
    EXPECT_EQ(ec.monthKeyCreated(), ec2->monthKeyCreated());
    EXPECT_STREQ(ec.getKeyIdentifier().c_str(), "");
    EXPECT_STREQ(ec2->getKeyIdentifier().c_str(), "keyIdentifier"); // When deserializing, the key identifier is stored in a member attribute.
}

std::string decryptMessage(model::EncryptionKey key, std::string_view base64Ciphertext)
{
    auto ciphertext = Base64::decodeToString(base64Ciphertext);
    const auto iv = ciphertext.substr(0, AesGcm256::IvLength);
    const auto authenticationTag =
        ciphertext.substr(ciphertext.size() - AesGcm256::AuthenticationTagLength, AesGcm256::AuthenticationTagLength);
    const auto plaintext =
        AesGcm256::decrypt(ciphertext.substr(AesGcm256::IvLength, ciphertext.size() - AesGcm256::IvLength -
                                                                      AesGcm256::AuthenticationTagLength),
                           key.messageKey(), iv, authenticationTag);
    return static_cast<std::string>(plaintext);
}

std::string_view unpadMessage(std::string_view message)
{
    size_t len = static_cast<size_t>(static_cast<uint8_t>(message[5]) | (static_cast<uint8_t>(message[4]) << 8));
    return message.substr(6 + len);
}

TEST_F(EncryptionKeyTest, keyDerivation)
{
    // data from https://github.com/gematik/gem-push-notifications-concept/blob/1.1/docs/concept.adoc#beispiel-f%C3%BCr-einen-austausch-im-oktober-2023
    const auto key = defaultKey();

    EXPECT_EQ(ByteHelper::toHex(key.sharedSecret()),
              "185fed66ea5cabbe00147bbd298b5dab0ed41b57ab254d35897b3a4504306e3b");
    EXPECT_EQ(ByteHelper::toHex(key.messageKey()), "3b4adcd58dea98db8e9cb0f5763fcd04fe932d67926cc04b20ba2a2f304ffff9");
    EXPECT_EQ(key.monthInfoString(), "2023-10");

    const auto nextKey = model::EncryptionKey::deriveFromPreviousMonthlyKey(key, "2023-11");
    EXPECT_EQ(ByteHelper::toHex(nextKey.sharedSecret()),
              "0c8662d90b04818afb317406fe7fcfcf8d103cd9bc6ad7847890d28620e85ec3");
    EXPECT_EQ(ByteHelper::toHex(nextKey.messageKey()),
              "39aa5dacd538f53f4b956d84c9b8f2e26933274d160b9fd1a263a27681c6331b");
    EXPECT_EQ(nextKey.monthInfoString(), "2023-11");

    ASSERT_EQ(model::EncryptionKey::deriveFromPreviousMonthlyKey(
                  model::EncryptionKey::deriveFromPreviousMonthlyKey(key, "2023-11"), "2023-12"),
              model::EncryptionKey::deriveFromPreviousMonthlyKey(key, "2023-12"));
}


TEST_F(EncryptionKeyTest, roundTrip)
{
    const auto key = defaultKey();

    auto plaintext = std::string{"hallo"};
    auto msg = key.encryptMessage(plaintext);

    const auto decrypted = decryptMessage(key, msg);
    ASSERT_EQ(decrypted.substr(0, 4), "PNM1");
    ASSERT_EQ(decrypted.size(), 1024);
    const auto unpadded = unpadMessage(decrypted);
    ASSERT_EQ(unpadded, plaintext);
}


TEST_F(EncryptionKeyTest, failPlaintextTooLarge)
{
    const auto key = defaultKey();
    auto plaintext = std::string(1025, 'a');

    ASSERT_ANY_THROW(key.encryptMessage(plaintext));
}


TEST_F(EncryptionKeyTest, invalidUsage)
{
    SafeString shortKey(ByteHelper::fromHex("f2ca"));
    SafeString sharedSecret(ByteHelper::fromHex("f2ca1bb6c7e907d06dafe4687e579fce76b37e4e93b7605022da52e6ccc26fd2"));
    std::string initialMonth("2023-10");
    // shared key too short
    ASSERT_ANY_THROW(model::EncryptionKey::deriveMonthlyKey(shortKey, initialMonth));
    // short message key
    ASSERT_ANY_THROW(model::EncryptionKey(sharedSecret, shortKey, "2025-01"));
    // invalid date
    ASSERT_ANY_THROW(model::EncryptionKey(sharedSecret, sharedSecret, "2025-13"));
}
