/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "erp/model/Health.hxx"

rapidjson::Pointer statusPointer("/status");
rapidjson::Pointer postgresStatusPointer("/checks/0/status");
rapidjson::Pointer hsmStatusPointer("/checks/2/status");
rapidjson::Pointer redisStatusPointer("/checks/1/status");
rapidjson::Pointer tslStatusPointer("/checks/3/status");
rapidjson::Pointer bnaStatusPointer("/checks/4/status");
rapidjson::Pointer idpStatusPointer("/checks/5/status");
rapidjson::Pointer seedTimerStatusPointer("/checks/6/status");
rapidjson::Pointer teeTokenUpdaterStatusPointer("/checks/7/status");


TEST_F(ErpWorkflowTest, HealthCheck)
{
    if ( client->getHostAddress() != "localhost")
    {
        VLOG(2) << "HealthCheck test skipped for cloud or production env";
        GTEST_SKIP();
    }

    using namespace std::string_literals;

    std::string outerPath = "/health";

    ClientRequest outerRequest(Header(HttpMethod::GET, std::move(outerPath), Header::Version_1_1,
                                      {{Header::Host, client->getHostHttpHeader()},
                                       {Header::UserAgent, "vau-cpp-it-test"s},
                                       {Header::ContentType, "application/octet-stream"}},
                                      HttpStatus::Unknown),
                               {});

    auto outerResponse = client->send(outerRequest);
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    const auto& body = outerResponse.getBody();
    ASSERT_FALSE(body.empty());
    rapidjson::Document healthDocument;
    healthDocument.Parse(body);

    ASSERT_FALSE(healthDocument.HasParseError());
    EXPECT_EQ(std::string(statusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(postgresStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(hsmStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(redisStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(tslStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(bnaStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(idpStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(seedTimerStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
    EXPECT_EQ(std::string(teeTokenUpdaterStatusPointer.Get(healthDocument)->GetString()), std::string(model::Health::up));
}
