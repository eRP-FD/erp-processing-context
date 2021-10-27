/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/ErpTypes.hxx"

#include <gtest/gtest.h>


class ErpTypesTest : public testing::Test
{
};




TEST_F(ErpTypesTest, ErpVector_create)
{
    const auto v = ErpVector::create("test");

    ASSERT_EQ(v.size(), 4);
    EXPECT_EQ(v[0], 't');
    EXPECT_EQ(v[1], 'e');
    EXPECT_EQ(v[2], 's');
    EXPECT_EQ(v[3], 't');
}


TEST_F(ErpTypesTest, ErpVector_equals)
{
    const auto u = ErpVector::create("test");
    const auto v = ErpVector::create("test");
    const auto w = ErpVector::create("something different");

    EXPECT_EQ(u,v);
    EXPECT_NE(v,w);
}


TEST_F(ErpTypesTest, ErpVector_startsWith)
{
    const auto u = ErpVector::create("test");
    const auto v = ErpVector::create("test text starting with test");

    EXPECT_TRUE(v.startsWith(u));
    EXPECT_TRUE(v.startsWith(v));
    EXPECT_TRUE(u.startsWith(u));
    EXPECT_FALSE(u.startsWith(v));
}


TEST_F(ErpTypesTest, ErpArray_create)
{
    const auto v = ErpArray<4>::create("test");

    ASSERT_EQ(v.size(), 4);
    EXPECT_EQ(v[0], 't');
    EXPECT_EQ(v[1], 'e');
    EXPECT_EQ(v[2], 's');
    EXPECT_EQ(v[3], 't');
}


TEST_F(ErpTypesTest, ErpArray_create_failForExcludedStringTerminator)
{
    // While zero bytes are allowed, this one is excluded by the string_view constructor and leads to a
    // four byte input argument which is rejected by ErpArray<5>.
    ASSERT_ANY_THROW(ErpArray<5>::create("test\0"));
}


TEST_F(ErpTypesTest, ErpArray_create_successForIncludedZeroByte)
{
    // Provide the string_view constructor with an explicit length that includes the zero byte which now is not
    // a string terminator anymore. Rather "test\0" is just some arbitrary binary data that happens to have a
    // zero byte in its last position.
    const auto v = ErpArray<5>::create(std::string_view("test\0", 5));

    ASSERT_EQ(v.size(), 5);
    EXPECT_EQ(v[0], 't');
    EXPECT_EQ(v[1], 'e');
    EXPECT_EQ(v[2], 's');
    EXPECT_EQ(v[3], 't');
    EXPECT_EQ(v[4], '\0');
}


TEST_F(ErpTypesTest, ErpArray_create_fail)
{
    ASSERT_ANY_THROW(ErpArray<5>::create("test"));
}