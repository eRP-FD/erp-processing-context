/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/common/BoostBeastMethod.hxx"

#include <gtest/gtest.h>


TEST(BoostBeastMethodTest, roundTripViaBeast)
{
    ASSERT_EQ(fromBoostBeastVerb(toBoostBeastVerb(HttpMethod::DELETE)), HttpMethod::DELETE);
    ASSERT_EQ(fromBoostBeastVerb(toBoostBeastVerb(HttpMethod::GET)), HttpMethod::GET);
    ASSERT_EQ(fromBoostBeastVerb(toBoostBeastVerb(HttpMethod::POST)), HttpMethod::POST);
}


TEST(BoostBeastMethodTest, roundTripViaErp)
{
    ASSERT_EQ(boost::beast::http::verb::delete_, toBoostBeastVerb(fromBoostBeastVerb(boost::beast::http::verb::delete_)));
    ASSERT_EQ(boost::beast::http::verb::get, toBoostBeastVerb(fromBoostBeastVerb(boost::beast::http::verb::get)));
    ASSERT_EQ(boost::beast::http::verb::post, toBoostBeastVerb(fromBoostBeastVerb(boost::beast::http::verb::post)));
}
