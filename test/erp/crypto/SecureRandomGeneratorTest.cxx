/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>
#include "erp/crypto/SecureRandomGenerator.hxx"


class SecureRandomGeneratorTest
    : public testing::Test
{
};

TEST_F(SecureRandomGeneratorTest, TestEntropy)
{
    ASSERT_EQ(SecureRandomGenerator::shannonEntropy("AAAAAA"), SecureRandomGenerator::shannonEntropy("BBBBBB"));
    ASSERT_EQ(SecureRandomGenerator::shannonEntropy(""), SecureRandomGenerator::shannonEntropy("A"));

    ASSERT_GT(SecureRandomGenerator::shannonEntropy("ABABAB"), SecureRandomGenerator::shannonEntropy("AAAAAA"));

    ASSERT_GT(
        SecureRandomGenerator::shannonEntropy("This is even more random"),
        SecureRandomGenerator::shannonEntropy("ABABAB"));

    ASSERT_GT(
        SecureRandomGenerator::shannonEntropy("XZSfSLMqs1oLpD7Ngya8Y59s"),
        SecureRandomGenerator::shannonEntropy("This is even more random"));

    ASSERT_GT(
        SecureRandomGenerator::shannonEntropy("eRRcKpveU5LSGarIZnTo+wGhMrMin5mBkBKskCetNV8v"),
        SecureRandomGenerator::shannonEntropy("XZSfSLMqs1oLpD7Ngya8Y59s"));

    ASSERT_NEAR(SecureRandomGenerator::shannonEntropy("Entropy"), 2.80735 * std::string("Entropy").size(), 0.001);
}


TEST_F(SecureRandomGeneratorTest, generate)
{
    SecureRandomGenerator::addEntropy(SafeString{"7fABYveIvSt4fNvqKaoDBjwzYFGSUGIkswMzYNIdnPRa"});
    auto random1 = SecureRandomGenerator::generate(32);
    auto random2 = SecureRandomGenerator::generate(32);

    ASSERT_EQ(random1.size(), 32);
    ASSERT_EQ(random2.size(), 32);
    ASSERT_NE(random1, random2);
}
