/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/beast/BoostBeastHeader.hxx"


#include <gtest/gtest.h>
#include <boost/beast/http/fields.hpp>


class BoostBeastHeaderTest : public testing::Test
{
};



TEST_F(BoostBeastHeaderTest, toString)
{
    boost::beast::http::fields fields;
    fields.set("foo", "bar");

    const auto convertedFields = BoostBeastHeader::convertHeaderFields(fields);

    EXPECT_EQ(convertedFields.find("foo")->second, "bar");
}
