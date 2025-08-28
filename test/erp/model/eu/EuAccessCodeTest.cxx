// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessCode.hxx"
#include "shared/model/ModelException.hxx"

#include <gtest/gtest.h>
TEST(EuAccessCodeTest, test)
{
    ASSERT_NO_THROW(model::EuAccessCode{SafeString{"aaaaaa"}});
    ASSERT_NO_THROW(model::EuAccessCode{SafeString{"123456"}});
    ASSERT_NO_THROW(model::EuAccessCode{SafeString{"A1b2D3"}});
    ASSERT_THROW(model::EuAccessCode{SafeString{""}}, model::ModelException);
    ASSERT_THROW(model::EuAccessCode{SafeString{"aaaaa"}}, model::ModelException);
    ASSERT_THROW(model::EuAccessCode{SafeString{"aaaaaaa"}}, model::ModelException);
    EXPECT_STREQ(model::EuAccessCode{SafeString{"huhu12"}}.toString().c_str(), "huhu12");
}
