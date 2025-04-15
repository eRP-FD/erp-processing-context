/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/message/HttpStatus.hxx"
#include "shared/util/ErpException.hxx"
#include "erp/util/search/PagingArgument.hxx"
#include "test/util/ErpMacros.hxx"

#include <gtest/gtest.h>


class PagingArgumentTest : public testing::Test
{
};


TEST_F(PagingArgumentTest, construction)
{
    PagingArgument argument;

    EXPECT_TRUE(argument.hasDefaultCount());
    EXPECT_EQ(argument.getCount(), PagingArgument::getDefaultCount());
    EXPECT_EQ(argument.getOffset(), 0);
}


TEST_F(PagingArgumentTest, count)
{
    PagingArgument argument;

    argument.setCount("11");
    EXPECT_EQ(argument.getCount(), 11);
    EXPECT_FALSE(argument.hasDefaultCount());
}


TEST_F(PagingArgumentTest, offset_failForInvalidNumber)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setOffset("ba98"), HttpStatus::BadRequest);
}


TEST_F(PagingArgumentTest, offset_failForTrailingCharacters)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setOffset("123 "), HttpStatus::BadRequest);
}


TEST_F(PagingArgumentTest, offset_failForNegativeValue)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setOffset("-123"), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, offset_succeedForZeroValue)
{
    PagingArgument argument;
    EXPECT_NO_THROW(argument.setOffset("0"));
}

TEST_F(PagingArgumentTest, offset_failForOutOfRange)
{
    auto outOfRangeValue = static_cast<intmax_t>(std::numeric_limits<int>::max()) + 1;
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setOffset(std::to_string(outOfRangeValue)), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, count_failForInvalidNumber)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setCount("ba98"), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, count_failForTrailingCharacters)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setCount("123 "), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, count_failForNegativeValue)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setCount("-123"), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, count_failForZeroValue)
{
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setCount("0"), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, count_failForOutOfRange)
{
    auto outOfRangeValue = static_cast<intmax_t>(std::numeric_limits<int>::max()) + 1;
    PagingArgument argument;
    EXPECT_ERP_EXCEPTION(argument.setCount(std::to_string(outOfRangeValue)), HttpStatus::BadRequest);
}

TEST_F(PagingArgumentTest, isSet)
{
    PagingArgument argument;
    EXPECT_FALSE(argument.isSet());

    argument.setOffset("3");
    EXPECT_TRUE(argument.isSet());

    argument.setOffset("0"); // back to the default
    EXPECT_FALSE(argument.isSet());

    argument.setCount(std::to_string(PagingArgument::getDefaultCount()));
    EXPECT_FALSE(argument.isSet());

    argument.setCount("4");
    EXPECT_TRUE(argument.isSet());
}


TEST_F(PagingArgumentTest, count_cappedAt50)
{
    PagingArgument argument;

    argument.setCount(std::to_string(PagingArgument::getDefaultCount() + 1));
    //                                                                   ^ one more than the maximum value.
    EXPECT_EQ(argument.getCount(), PagingArgument::getDefaultCount());
}
