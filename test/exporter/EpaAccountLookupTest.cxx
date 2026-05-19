/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookup.hxx"
#include "test/exporter/mock/EpaAccountLookupClientMock.hxx"

#include <gtest/gtest.h>

#include "shared/util/Configuration.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

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
        lookup.lookup("x-request-id", model::Kvnr("X123456788"), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::allowed);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
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
        lookup.lookup("x-request-id", model::Kvnr("X123456788"), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::deny);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
}

TEST_F(EpaAccountLookupTest, notFound)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::NotFound);
    client->setResponseBody(R"_()_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788"), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::notFound);
    EXPECT_EQ(epaAccount.host, "");
    EXPECT_EQ(epaAccount.port, 0);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
}

TEST_F(EpaAccountLookupTest, unknown)
{
    auto client = std::make_unique<EpaAccountLookupClientMock>();
    client->setResponseStatus(HttpStatus::InternalServerError);
    client->setResponseBody(R"_(uh oh)_");
    EpaAccountLookup lookup(std::move(client));
    auto epaAccount =
        lookup.lookup("x-request-id", model::Kvnr("X123456788"), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::unknown);
    EXPECT_EQ(epaAccount.host, "");
    EXPECT_EQ(epaAccount.port, 0);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
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
        lookup.lookup("x-request-id", model::Kvnr("X123456788"), {std::make_tuple("huhu", 3)});
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::conflict);
    EXPECT_EQ(epaAccount.host, "huhu");
    EXPECT_EQ(epaAccount.port, 3);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
}

TEST_F(EpaAccountLookupTest, allowedCached)
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

    std::string str = "";
    for (size_t i = 0; i < 10; ++i)
    {
        if (str.size())
        {
            str += ";";
        }
        str += "server" + std::to_string(i) + ".testing.example.com:" + std::to_string((1 + i) * 1000 + i);
    }
    EnvironmentVariableGuard featureToggleGuard(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN,
                                                str);
    auto fqdns = Configuration::instance().epaFQDNs();

    EpaAccountLookup lookup(std::move(client));
    auto epaAccount = lookup.lookup("x-request-id", model::Kvnr("X123456788"), "server7");
    EXPECT_EQ(epaAccount.lookupResult, EpaAccount::Code::allowed);
    EXPECT_EQ(epaAccount.host, "server7.testing.example.com");
    EXPECT_EQ(epaAccount.port, 8007);
    EXPECT_EQ(epaAccount.kvnr, model::Kvnr("X123456788"));
}
