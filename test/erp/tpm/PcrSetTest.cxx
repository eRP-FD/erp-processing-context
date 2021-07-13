#include "erp/tpm/PcrSet.hxx"

#include <gtest/gtest.h>

class PcrSetTest : public testing::Test
{
};


TEST_F(PcrSetTest, fromString)
{
    const auto set = PcrSet::fromString("1, 2,   3   ");

    ASSERT_EQ(set.size(), 3);
    auto iterator = set.begin();
    ASSERT_EQ(*iterator++, 1);
    ASSERT_EQ(*iterator++, 2);
    ASSERT_EQ(*iterator++, 3);
    ASSERT_EQ(iterator, set.end());
}


TEST_F(PcrSetTest, fromString_withBrackets)
{
    const auto set = PcrSet::fromString("[1, 2,   3   ]");

    ASSERT_EQ(set.size(), 3);
    auto iterator = set.begin();
    ASSERT_EQ(*iterator++, 1);
    ASSERT_EQ(*iterator++, 2);
    ASSERT_EQ(*iterator++, 3);
    ASSERT_EQ(iterator, set.end());
}


TEST_F(PcrSetTest, fromString_failForNegativeValue)
{
    ASSERT_ANY_THROW(
        PcrSet::fromString("1,-2"));
}


TEST_F(PcrSetTest, fromString_failForLargeValue)
{
    ASSERT_ANY_THROW(
        PcrSet::fromString("1,16"));
}


TEST_F(PcrSetTest, fromList)
{
    const auto set = PcrSet::fromList(std::vector<size_t>{1, 2, 3});

    ASSERT_EQ(set.size(), 3);
    auto iterator = set.begin();
    ASSERT_EQ(*iterator++, 1);
    ASSERT_EQ(*iterator++, 2);
    ASSERT_EQ(*iterator++,   3);
    ASSERT_EQ(iterator, set.end());
}


TEST_F(PcrSetTest, operatorEq)
{
    const auto setA = PcrSet::fromString("3,2,1");
    const auto setB = PcrSet::fromList(std::vector<size_t>{1, 3, 2});

    ASSERT_TRUE(setA == setB);

    const auto setC = PcrSet::fromString("1,2");

    ASSERT_FALSE(setA == setC);
}


TEST_F(PcrSetTest, toString_withBrackets)
{
    ASSERT_EQ(PcrSet::fromString("3,2,1").toString(true), "[1,2,3]");
}


TEST_F(PcrSetTest, toString_withoutBrackets)
{
    ASSERT_EQ(PcrSet::fromString("3,2,1").toString(false), "1,2,3");
}