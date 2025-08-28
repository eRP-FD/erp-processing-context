// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessPermission.hxx"

#include <gtest/gtest.h>

TEST(EuAccessPermissionTest, test)
{
    std::optional<model::EuAccessPermission> euaccessPermission;
    auto now = model::Timestamp::now();
    ASSERT_NO_THROW((euaccessPermission = model::EuAccessPermission{model::EuAccessCode{SafeString{"abcdef"}},
                                                                    model::CountryCode{"FR"}}));
    EXPECT_EQ(euaccessPermission->getCountryCode().toString(), "FR");
    EXPECT_STREQ(euaccessPermission->getAccessCode().toString().c_str(), "abcdef");
    EXPECT_GE(euaccessPermission->getCreatedAt(), now);
    EXPECT_EQ(euaccessPermission->getValidUntil(), euaccessPermission->getCreatedAt() + std::chrono::hours{1});
}


TEST(EuAccessPermissionTest, test2)
{
    std::optional<model::EuAccessPermission> euaccessPermission;
    auto created = model::Timestamp::now() - std::chrono::minutes{30};
    auto validUntil = model::Timestamp::now() + std::chrono::minutes{30};
    ASSERT_NO_THROW((euaccessPermission = model::EuAccessPermission{model::EuAccessCode{SafeString{"abcdef"}},
                                                                    model::CountryCode{"FR"}, validUntil}));
    EXPECT_EQ(euaccessPermission->getCountryCode().toString(), "FR");
    EXPECT_STREQ(euaccessPermission->getAccessCode().toString().c_str(), "abcdef");
    EXPECT_EQ(euaccessPermission->getCreatedAt(), created);
    EXPECT_EQ(euaccessPermission->getValidUntil(), validUntil);
}