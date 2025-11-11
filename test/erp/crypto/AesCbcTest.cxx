/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/AesCbc.hxx"
#include "shared/util/ByteHelper.hxx"

#include <gtest/gtest.h>


template<typename AesCbcT>
void test(const std::string& keyHex, const std::string& ivHex, const std::string& plaintextHex,
          const std::string& ciphertextHex)
{
    // Given.
    const auto symmetricKey = SafeString(ByteHelper::fromHex(keyHex));
    const std::string iv = ByteHelper::fromHex(ivHex);
    const std::string plaintext = ByteHelper::fromHex(plaintextHex);

    // Encrypt.
    const auto ciphertext = AesCbcT::encrypt(plaintext, symmetricKey, iv);
    ASSERT_EQ(ciphertext, ByteHelper::fromHex(ciphertextHex));
    ASSERT_EQ(plaintext.length(), ciphertext.length());

    // Decrypt.
    const auto decryptedPlaintext = AesCbcT::decrypt(ciphertext, symmetricKey, iv);

    ASSERT_EQ(std::string(static_cast<const char*>(decryptedPlaintext), decryptedPlaintext.size()), plaintext)
        << "plaintextHex was " << plaintextHex;
}

TEST(AesCbc128Test, encrypt)
{
    // Test test cases from https://boringssl.googlesource.com/boringssl/+/refs/heads/main/crypto/cipher/test/cipher_tests.txt
    test<AesCbc128>("2B7E151628AED2A6ABF7158809CF4F3C", "000102030405060708090A0B0C0D0E0F",
                    "6BC1BEE22E409F96E93D7E117393172A", "7649ABAC8119B246CEE98E9B12E9197D");
    test<AesCbc128>("2B7E151628AED2A6ABF7158809CF4F3C", "7649ABAC8119B246CEE98E9B12E9197D",
                    "AE2D8A571E03AC9C9EB76FAC45AF8E51", "5086CB9B507219EE95DB113A917678B2");
    test<AesCbc128>("2B7E151628AED2A6ABF7158809CF4F3C", "5086CB9B507219EE95DB113A917678B2",
                    "30C81C46A35CE411E5FBC1191A0A52EF", "73BED6B8E3C1743B7116E69E22229516");
    test<AesCbc128>("2B7E151628AED2A6ABF7158809CF4F3C", "73BED6B8E3C1743B7116E69E22229516",
                    "F69F2445DF4F9B17AD2B417BE66C3710", "3FF1CAA1681FAC09120ECA307586E1A7");
    test<AesCbc128>("2B7E151628AED2A6ABF7158809CF4F3C", "73BED6B8E3C1743B7116E69E22229516", "", "");
}


TEST(AesCbc256Test, encrypt)
{
    // Test test cases from https://boringssl.googlesource.com/boringssl/+/refs/heads/main/crypto/cipher/test/cipher_tests.txt
    test<AesCbc256>("603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4",
                    "000102030405060708090A0B0C0D0E0F", "6BC1BEE22E409F96E93D7E117393172A",
                    "F58C4C04D6E5F1BA779EABFB5F7BFBD6");
    test<AesCbc256>("603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4",
                    "F58C4C04D6E5F1BA779EABFB5F7BFBD6", "AE2D8A571E03AC9C9EB76FAC45AF8E51",
                    "9CFC4E967EDB808D679F777BC6702C7D");
    test<AesCbc256>("603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4",
                    "9CFC4E967EDB808D679F777BC6702C7D", "30C81C46A35CE411E5FBC1191A0A52EF",
                    "39F23369A9D9BACFA530E26304231461");
    test<AesCbc256>("603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4",
                    "39F23369A9D9BACFA530E26304231461", "F69F2445DF4F9B17AD2B417BE66C3710",
                    "B2EB05E2C39BE9FCDA6C19078C6A9D1B");
    test<AesCbc256>("603DEB1015CA71BE2B73AEF0857D77811F352C073B6108D72D9810A30914DFF4",
                    "39F23369A9D9BACFA530E26304231461", "", "");
}
