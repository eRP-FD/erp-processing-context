/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "src/shared/model/Health.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

namespace {
rapidjson::Pointer statusPointer("/status");
rapidjson::Pointer postgresStatusPointer("/checks/0/status");
rapidjson::Pointer hsmStatusPointer("/checks/2/status");
rapidjson::Pointer redisStatusPointer("/checks/1/status");
rapidjson::Pointer tslStatusPointer("/checks/3/status");
rapidjson::Pointer bnaStatusPointer("/checks/4/status");
rapidjson::Pointer idpStatusPointer("/checks/5/status");
rapidjson::Pointer seedTimerStatusPointer("/checks/6/status");
rapidjson::Pointer teeTokenUpdaterStatusPointer("/checks/7/status");
}

class ErpHealthTest : public ::testing::TestWithParam<TestClient::Target> {
public:
    void SetUp() override
    {
        mClient = TestClient::create(StaticData::getXmlValidator(), GetParam());
    }
protected:
    std::unique_ptr<TestClient> mClient;
};

TEST_P(ErpHealthTest, HealthCheck)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    auto hostaddress = mClient->getHostAddress();
    if (hostaddress != "localhost" && hostaddress != "127.0.0.1")
    {
        GTEST_SKIP();
        return;
    }

    std::string outerPath = "/health";

    ClientRequest request(Header(HttpMethod::GET, std::move(outerPath), Header::Version_1_1,
                                      {{Header::Host, mClient->getHostHttpHeader()},
                                       {Header::UserAgent, "eRp-Testsuite/1.0.0 GMTIK/JMeter-Internet"s},
                                       {Header::ContentType, "application/octet-stream"}},
                                      HttpStatus::Unknown),
                               {});

    auto outerResponse = mClient->send(request);
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    const auto& body = outerResponse.getBody();
    ASSERT_FALSE(body.empty());
    rapidjson::Document healthDocument;
    healthDocument.Parse(body);

    static std::set healthStates{model::Health::up, model::Health::down};

    ASSERT_FALSE(healthDocument.HasParseError());
    EXPECT_TRUE(healthStates.count(statusPointer.Get(healthDocument)->GetString()))
                << statusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(postgresStatusPointer.Get(healthDocument)->GetString()))
                << postgresStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(hsmStatusPointer.Get(healthDocument)->GetString()))
                << hsmStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(redisStatusPointer.Get(healthDocument)->GetString()))
                << redisStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(tslStatusPointer.Get(healthDocument)->GetString()))
                << tslStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(bnaStatusPointer.Get(healthDocument)->GetString()))
                << bnaStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(idpStatusPointer.Get(healthDocument)->GetString()))
                << idpStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(seedTimerStatusPointer.Get(healthDocument)->GetString()))
                << seedTimerStatusPointer.Get(healthDocument)->GetString();
    EXPECT_TRUE(healthStates.count(teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString()))
                << teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString();
}


INSTANTIATE_TEST_SUITE_P(ports, ErpHealthTest, testing::Values(TestClient::Target::ADMIN, TestClient::Target::VAU),
                         [](const auto& info) {
                             return to_string(info.param);
                         });
