/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/util/Environment.hxx"

class EnvironmentTest : public testing::Test
{
public:
    static constexpr auto envVar1 = "ERP_ENVIRONMENT_VARIABLE_TESTVAR1";
    static constexpr auto envVar2 = "ERP_ENVIRONMENT_VARIABLE_TESTVAR2";
    void SetUp() override
    {
        Environment::unset(envVar1);
        Environment::unset(envVar2);
    }
};

TEST_F(EnvironmentTest, set_get_unset)//NOLINT(readability-function-cognitive-complexity)
{
    Environment::set(envVar1, "VALUE");
    ASSERT_TRUE(Environment::get(envVar1).has_value());
    EXPECT_EQ(Environment::get(envVar1).value(), "VALUE");
    EXPECT_FALSE(Environment::get(envVar2).has_value());

    Environment::set(envVar2, "OTHER VALUE");
    ASSERT_TRUE(Environment::get(envVar1).has_value());
    EXPECT_EQ(Environment::get(envVar1).value(), "VALUE");
    ASSERT_TRUE(Environment::get(envVar2).has_value());
    EXPECT_EQ(Environment::get(envVar2).value(), "OTHER VALUE");

    Environment::unset(envVar1);
    EXPECT_FALSE(Environment::get(envVar1).has_value());
    ASSERT_TRUE(Environment::get(envVar2).has_value());
    EXPECT_EQ(Environment::get(envVar2).value(), "OTHER VALUE");

    Environment::unset(envVar2);
    EXPECT_FALSE(Environment::get(envVar1).has_value());
    EXPECT_FALSE(Environment::get(envVar2).has_value());
}

TEST_F(EnvironmentTest, getString)
{
    EXPECT_EQ(Environment::getString(envVar1, "default1"), "default1");
    EXPECT_EQ(Environment::getString(envVar1, "default2"), "default2");

    Environment::set(envVar1, "some string");
    EXPECT_EQ(Environment::getString(envVar1, "default1"), "some string");
    EXPECT_EQ(Environment::getString(envVar1, "default2"), "some string");
}

TEST_F(EnvironmentTest, getInt)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_EQ(Environment::getInt(envVar1, 100), 100);
    EXPECT_EQ(Environment::getInt(envVar1, 200), 200);

    Environment::set(envVar1, "300");
    EXPECT_EQ(Environment::getInt(envVar1, 100), 300);
    EXPECT_EQ(Environment::getInt(envVar1, 200), 300);

    Environment::set(envVar1, "X");
    EXPECT_EQ(Environment::getInt(envVar1, 100), 100);
    EXPECT_EQ(Environment::getInt(envVar1, 200), 200);

    Environment::set(envVar1, "300X");
    EXPECT_EQ(Environment::getInt(envVar1, 100), 100);
    EXPECT_EQ(Environment::getInt(envVar1, 200), 200);
}


TEST_F(EnvironmentTest, getBool)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "true");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), true);

    Environment::set(envVar1, "yes");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), true);

    Environment::set(envVar1, "1");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), true);

    Environment::set(envVar1, "TrUe");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), true);

    Environment::set(envVar1, "false");
    EXPECT_EQ(Environment::getBool(envVar1, true), false);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "no");
    EXPECT_EQ(Environment::getBool(envVar1, true), false);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "0");
    EXPECT_EQ(Environment::getBool(envVar1, true), false);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "FaLsE");
    EXPECT_EQ(Environment::getBool(envVar1, true), false);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "falser");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "truer");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "may be!");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);

    Environment::set(envVar1, "2");
    EXPECT_EQ(Environment::getBool(envVar1, true), true);
    EXPECT_EQ(Environment::getBool(envVar1, false), false);
}

