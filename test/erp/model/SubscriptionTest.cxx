/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ModelException.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/Subscription.hxx"

#include <gtest/gtest.h>

TEST(SubscriptionTest, DefaultConstruct)
{
    const auto* json =
        R"--({ "resourceType": "Subscription", "status": "requested", "criteria": "Communication?received=null&recipient=recipient1", "channel": { "type": "websocket" } })--";
    auto subscription = model::Subscription::fromJsonNoValidation(json);
    EXPECT_EQ(subscription.recipientId(), "recipient1");
    EXPECT_EQ(subscription.status(), model::Subscription::Status::Requested);
}

TEST(SubscriptionTest, ConstructorWithoutRecipient)
{
    const auto* json =
        R"--({ "resourceType": "Subscription", "status": "requested", "criteria": "Communication?received=null", "channel": { "type": "websocket" } })--";
    auto subscription = model::Subscription::fromJsonNoValidation(json);
    EXPECT_EQ(subscription.recipientId(), "");
    EXPECT_EQ(subscription.status(), model::Subscription::Status::Requested);
}

TEST(SubscriptionTest, ConstructorWithoutStatusAndRecipient)
{
    const auto* json =
        R"--({ "resourceType": "Subscription", "criteria": "Communication?received=null", "channel": { "type": "websocket" } })--";
    auto subscription = model::Subscription::fromJsonNoValidation(json);
    EXPECT_EQ(subscription.recipientId(), "");
    EXPECT_EQ(subscription.status(), model::Subscription::Status::Unknown);
}

TEST(SubscriptionTest, UpdateStatus)
{
    const auto* json =
        R"--({ "resourceType": "Subscription", "status": "requested", "criteria": "Communication?received=null&recipient=recipient1", "channel": { "type": "websocket" } })--";
    auto subscription = model::Subscription::fromJsonNoValidation(json);
    EXPECT_EQ(subscription.recipientId(), "recipient1");
    EXPECT_EQ(subscription.status(), model::Subscription::Status::Requested);
    subscription.setStatus(model::Subscription::Status::Active);
    EXPECT_TRUE(subscription.status() == model::Subscription::Status::Active);
}
