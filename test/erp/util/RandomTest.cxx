/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Random.hxx"

#include <gtest/gtest.h>
#include <iostream>

TEST(RandomTest, randomIntBetween)
{
    for(int from = -50; from <= 50; ++from)
    {
        for(int to = from + 1; to <= from + 30; ++to)
        {
            int r = Random::randomIntBetween(from, to);
            ASSERT_GE(r, from);
            ASSERT_LT(r, to);
        }
    }

    // error cases:
    ASSERT_ANY_THROW(Random::randomIntBetween(1, 1));
    ASSERT_ANY_THROW(Random::randomIntBetween(1, -1));
}


TEST(RandomTest, randomBinaryData)
{
    auto size = 65536;
    util::Buffer buffer = Random::randomBinaryData(size);
    ASSERT_EQ(buffer.size(), size);
}