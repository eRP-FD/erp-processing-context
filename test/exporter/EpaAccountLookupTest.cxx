/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookup.hxx"
#include "test/exporter/mock/EpaAccountLookupClientMock.hxx"

#include <gtest/gtest.h>

class EpaAccountLookupTest : public testing::Test
{
};

TEST_F(EpaAccountLookupTest, allowed)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::OK);
    client->setResponseBody(R"_(
{
  "data": [
    {
      "functionId": "medication",
      "decision": "deny"
    },
    {
      "functionId": "erp-submission",
      "decision": "permit"
    }
  ]
}
)_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788", model::Kvnr::Type::gkv), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::allowed);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788", model::Kvnr::Type::gkv));
}

TEST_F(EpaAccountLookupTest, denied)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::OK);
    client->setResponseBody(R"_(
{
  "data": [
    {
      "functionId": "medication",
      "decision": "deny"
    },
    {
      "functionId": "erp-submission",
      "decision": "deny"
    }
  ]
}
)_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788", model::Kvnr::Type::gkv), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::deny);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788", model::Kvnr::Type::gkv));
}

TEST_F(EpaAccountLookupTest, notFound)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::NotFound);
    client->setResponseBody(R"_()_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788", model::Kvnr::Type::gkv), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::notFound);
    EXPECT_EQ(epaAccount.host, "");
    EXPECT_EQ(epaAccount.port, 0);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788", model::Kvnr::Type::gkv));
}

TEST_F(EpaAccountLookupTest, unknown)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::InternalServerError);
    client->setResponseBody(R"_(uh oh)_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788", model::Kvnr::Type::gkv), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::unknown);
    EXPECT_EQ(epaAccount.host, "");
    EXPECT_EQ(epaAccount.port, 0);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788", model::Kvnr::Type::gkv));
}

TEST_F(EpaAccountLookupTest, conflict)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::Conflict);
    client->setResponseBody(R"_({
"errorCode": "statusMismatch"
})_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788", model::Kvnr::Type::gkv), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::conflict);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788", model::Kvnr::Type::gkv));
}
