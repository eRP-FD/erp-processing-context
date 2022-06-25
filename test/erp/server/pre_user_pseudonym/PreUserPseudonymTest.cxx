/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/pc/pre_user_pseudonym/PreUserPseudonym.hxx"

TEST(PreUserPseudonymTest, construct) // NOLINT
{
    CmacSignature sig = CmacSignature::fromHex("0123456789ABCDEF0123456789ABCDEF");
    PreUserPseudonym pup{sig};
    EXPECT_EQ(sig, pup.getSignature());
}

TEST(PreUserPseudonymTest, fromUserPseudonym) // NOLINT
{
    CmacSignature sig = CmacSignature::fromHex("0123456789ABCDEF0123456789ABCDEF");
    PreUserPseudonym pup = PreUserPseudonym::fromUserPseudonym("0123456789ABCDEF0123456789ABCDEF-FEDCBA9876543210FEDCBA9876543210");
    EXPECT_EQ(sig, pup.getSignature());
}


TEST(PreUserPseudonymTest, fromUserPseudonym_errors) // NOLINT
{
    const char* const empty          = "";
    const char* const veryShort      = "01234";
    const char* const tooShort       = "0123456789ABCDEF0123456789ABCDEF-FEDCBA9876543210FEDCBA987654321";
    const char* const tooLong        = "0123456789ABCDEF0123456789ABCDEF-FEDCBA9876543210FEDCBA98765432101";
    const char* const wrongDelimiter = "0123456789ABCDEF0123456789ABCDEF+FEDCBA9876543210FEDCBA9876543210";
    const char* const invalidPup     = "X123456789ABCDEF0123456789ABCDEF-FEDCBA9876543210FEDCBA9876543210";
    const char* const invalidUpCmac  = "0123456789ABCDEF0123456789ABCDEF-XEDCBA9876543210FEDCBA9876543210";
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(empty));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(veryShort));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(tooShort));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(tooLong));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(wrongDelimiter));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(invalidPup));
    EXPECT_ANY_THROW((void)PreUserPseudonym::fromUserPseudonym(invalidUpCmac));
}
