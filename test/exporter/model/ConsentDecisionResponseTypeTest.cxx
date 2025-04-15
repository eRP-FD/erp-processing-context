/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/ModelException.hxx"
#include "exporter/model/ConsentDecisionsResponseType.hxx"

#include <gtest/gtest.h>

class ConsentDecisionResponseTypeTest : public testing::Test
{
public:
};

TEST_F(ConsentDecisionResponseTypeTest, parseSample)
{
    std::string json = R"_(
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
)_";

    std::optional<model::ConsentDecisionsResponseType> consentDecisionsResponse;
    ASSERT_NO_THROW(consentDecisionsResponse.emplace(model::ConsentDecisionsResponseType{json}));
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("medication"),
              model::ConsentDecisionsResponseType::Decision::deny);
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("erp-submission"),
              model::ConsentDecisionsResponseType::Decision::permit);
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("invalid"), std::nullopt);
}

TEST_F(ConsentDecisionResponseTypeTest, parseBad1)
{
    std::string json = R"_(
{
  "data": {
    {
      "functionId": "medication",
      "decision": "deny"
    },
    {
      "functionId": "erp-submission",
      "decision": "permit"
    }
  }
}
)_";
    ASSERT_THROW(model::ConsentDecisionsResponseType{json}, model::ModelException);
}

TEST_F(ConsentDecisionResponseTypeTest, parseBad2)
{
    std::string json = R"_(
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
)_";
    ASSERT_THROW(model::ConsentDecisionsResponseType{json}, model::ModelException);
}

TEST_F(ConsentDecisionResponseTypeTest, parseBad3)
{
    std::string json = R"_(
{}
)_";
    ASSERT_THROW(model::ConsentDecisionsResponseType{json}, model::ModelException);
}

TEST_F(ConsentDecisionResponseTypeTest, parseGood)
{
    std::string json = R"_(
{
  "data": []
}
)_";

    std::optional<model::ConsentDecisionsResponseType> consentDecisionsResponse;
    ASSERT_NO_THROW(consentDecisionsResponse.emplace(model::ConsentDecisionsResponseType{json}));
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("medication"), std::nullopt);
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("erp-submission"), std::nullopt);
    EXPECT_EQ(consentDecisionsResponse->getDecisionFor("invalid"), std::nullopt);
}
