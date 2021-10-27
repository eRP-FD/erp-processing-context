/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/common/BoostBeastHttpStatus.hxx"

#include <gtest/gtest.h>


TEST(BoostBeastHttpStatusTest, boostRoundTrip)
{
#define STATUS(boostStatus, eStatus, code)                                        \
    {                                                                             \
        const auto computedBoostStatus = toBoostBeastStatus(HttpStatus::eStatus); \
        ASSERT_EQ(boost::beast::http::status::boostStatus, computedBoostStatus);  \
        const auto computedEStatus = fromBoostBeastStatus(computedBoostStatus);   \
        ASSERT_EQ(HttpStatus::eStatus, computedEStatus);                          \
    }
#include "erp/common/HttpStatus.inc.hxx"
#undef STATUS
}


TEST(BoostBeastHttpStatusTest, epaRoundTrip)
{
#define STATUS(boostStatus, eStatus, code)                                                          \
    {                                                                                               \
        const auto computedEStatus = fromBoostBeastStatus(boost::beast::http::status::boostStatus); \
        ASSERT_EQ(HttpStatus::eStatus, computedEStatus);                                            \
        const auto computedBoostStatus = toBoostBeastStatus(computedEStatus);                       \
        ASSERT_EQ(boost::beast::http::status::boostStatus, computedBoostStatus);                    \
    }
#include "erp/common/HttpStatus.inc.hxx"
#undef STATUS
}
