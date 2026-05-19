/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/renderer/Localization.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

using namespace dosagetext;

class LocalizationTest : public testing::Test
{
};

TEST_F(LocalizationTest, germanDecimalTest)
{
    EXPECT_EQ("0", germanDecimal(".0"));
    EXPECT_EQ("0,05", germanDecimal(".0500"));
    EXPECT_EQ("0", germanDecimal("0.0"));
    EXPECT_EQ("0", germanDecimal("0.00000"));
    EXPECT_EQ("1", germanDecimal("1.0"));
    EXPECT_EQ("1", germanDecimal("1."));
    EXPECT_EQ("1", germanDecimal("0001."));
    EXPECT_EQ("0,0001", germanDecimal("0.000100"));
    EXPECT_EQ("123", germanDecimal("123"));
    EXPECT_EQ("1", germanDecimal("0001"));
    EXPECT_EQ("1", germanDecimal("0001.0"));
    EXPECT_EQ("0", germanDecimal("0000"));
    EXPECT_EQ("0", germanDecimal("0000.0"));
    EXPECT_EQ("123,4", germanDecimal("123.40"));
    EXPECT_EQ("1,1", germanDecimal("01.10"));
    EXPECT_EQ("-1,1", germanDecimal("-01.10"));
    EXPECT_EQ("1,1", germanDecimal("+01.10"));
    EXPECT_EQ("0", germanDecimal("+0.0"));
    EXPECT_EQ("0,5", germanDecimal(".5"));
    EXPECT_EQ("0,05", germanDecimal(".0500"));
    EXPECT_EQ("1", germanDecimal("0001."));
    EXPECT_EQ("0", germanDecimal("0000."));
    EXPECT_EQ("0", germanDecimal("-0"));
    EXPECT_EQ("0", germanDecimal("-0.0"));
    EXPECT_EQ("0", germanDecimal("-.0"));
    EXPECT_EQ("-0,01", germanDecimal("-.01"));
    EXPECT_EQ("0,00000001", germanDecimal("0.0000000100"));
    EXPECT_THROW(germanDecimal("0.0.0"), model::ModelException);
}