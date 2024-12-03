/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/RapidjsonDocument.hxx"

#include <gtest/gtest.h>


class RapidjsonDocumentTest : public testing::Test
{
};




TEST_F(RapidjsonDocumentTest, distinctInstances)
{
    struct A;
    struct B;
    RapidjsonDocument<A> a;
    RapidjsonDocument<B> b;

    ASSERT_EQ(&*a, &*a);
    ASSERT_EQ(&*b, &*b);

    // Verify that two distinct instances of RapidjsonDocument return two distinct instance of rapidjson::Document
    ASSERT_NE(&*a, &*b);
}
