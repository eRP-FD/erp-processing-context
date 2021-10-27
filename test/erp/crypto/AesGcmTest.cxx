/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/crypto/AesGcm.hxx"
#include "erp/util/ByteHelper.hxx"


class AesGcm128Test : public testing::Test
{
};


TEST_F(AesGcm128Test, encryptDecrypt)
{
    // Given
    std::string plaintext = "this is the plain text that will be first encrypted, then decrypted";
    const auto symmetricKey = SafeString("0123456789abcdef");
    std::string iv = "012345678912";

    // Encrypt, then decrypt.
    const auto [ciphertext, authenticationTag] = AesGcm128::encrypt(plaintext, symmetricKey, iv);
    const auto decryptedPlaintext = AesGcm128::decrypt(ciphertext, symmetricKey, iv, authenticationTag);

    ASSERT_EQ(static_cast<std::string_view>(decryptedPlaintext), plaintext);
}

TEST(AesGcm256Test, encryptDecrypt)
{
    // Given
    std::string plaintext = "this is the plain text that will be first encrypted, then decrypted";
    const auto symmetricKey = SafeString("0123456789abcdefghijklmnopqrstuv");
    std::string iv = "012345678912";

    // Encrypt, then decrypt.
    const auto [ciphertext, authenticationTag] = AesGcm256::encrypt(plaintext, symmetricKey, iv);
    const auto decryptedPlaintext = AesGcm256::decrypt(ciphertext, symmetricKey, iv, authenticationTag);

    ASSERT_EQ(static_cast<std::string_view>(decryptedPlaintext), plaintext);
}

template<class AesGcmT>
void test(const std::string& keyHex, const std::string& ivHex, const std::string& plaintextHex,
          const std::string& ciphertextHex, const std::string& authenticationTagHex)
{
    // Given.
    const auto symmetricKey = SafeString(ByteHelper::fromHex(keyHex));
    const std::string iv = ByteHelper::fromHex(ivHex);
    const std::string plaintext = ByteHelper::fromHex(plaintextHex);

    // Encrypt.
    const auto [ciphertext, authenticationTag] = AesGcmT::encrypt(plaintext, symmetricKey, iv);
    ASSERT_EQ(ciphertext, ByteHelper::fromHex(ciphertextHex));
    ASSERT_EQ(authenticationTag, ByteHelper::fromHex(authenticationTagHex));
    ASSERT_EQ(plaintext.length(), ciphertext.length());

    // Decrypt.
    const auto decryptedPlaintext = AesGcmT::decrypt(ciphertext, symmetricKey, iv, authenticationTag);

    ASSERT_EQ(std::string(static_cast<const char*>(decryptedPlaintext), decryptedPlaintext.size()), plaintext)
        << "plaintextHex was " << plaintextHex;
}


TEST_F(AesGcm128Test, encrypt)
{
    // Test test cases from https://boringssl.googlesource.com/boringssl/+/2214/crypto/cipher/cipher_test.txt
    test<AesGcm128>("00000000000000000000000000000000", "000000000000000000000000", "","", "58e2fccefa7e3061367f1d57a4e7455a");
    test<AesGcm128>("00000000000000000000000000000000", "000000000000000000000000", "00000000000000000000000000000000", "0388dace60b6a392f328c2b971b2fe78", "ab6e47d42cec13bdf53a67b21257bddf");
    test<AesGcm128>("feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888", "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255", "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985", "4d5c2af327cd64a62cf35abd2ba6fab4");
}

TEST(AesGcm256Test, encrypt)
{
    // Test test cases from https://boringssl.googlesource.com/boringssl/+/2214/crypto/cipher/cipher_test.txt
    test<AesGcm256>("0000000000000000000000000000000000000000000000000000000000000000", "000000000000000000000000", "","", "530f8afbc74536b9a963b4f1c4cb738b");
    test<AesGcm256>("0000000000000000000000000000000000000000000000000000000000000000", "000000000000000000000000", "00000000000000000000000000000000", "cea7403d4d606b6e074ec5d3baf39d18", "d0d1c8a799996bf0265b98b5d48ab919");
    test<AesGcm256>("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888", "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255", "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad", "b094dac5d93471bdec1a502270e3cc6c");
}
