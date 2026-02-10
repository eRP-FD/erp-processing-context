/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
* non-exclusively licensed to gematik GmbH
*/

#include "exporter/BdeMessage.hxx"
#include "shared/util/String.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>


TEST(BDEMessageTest, innerOperationMapping_MappedPath)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    testing::internal::CaptureStderr();
    bdeMessage->update(BDEMessage::Data{.innerOperation = "/ords/rezepte/oauth/token"});
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find(R"-("operation":"ERP.UC_5_7")-") != std::string::npos);
}

TEST(BDEMessageTest, innerOperationMapping_UnmappedPath)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    testing::internal::CaptureStderr();
    bdeMessage->update(BDEMessage::Data{.innerOperation = "/foo/bar"});
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find(R"-("operation":"")-") != std::string::npos);
}

TEST(BDEMessageTest, stripErrorCodePrefix)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    testing::internal::CaptureStderr();
    bdeMessage->update(BDEMessage::Data{.errorCode = "MEDICATIONSVC_SOME_ERROR"});
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find(R"-("error_component":"SOME_ERROR")-") != std::string::npos);
}

TEST(BDEMessageTest, test0)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    testing::internal::CaptureStderr();
    std::string startTimeStr = R"-("timestamp":"##VAL##")-";
    boost::replace_all(startTimeStr, "##VAL##", bdeMessage->getData().startTime.value().toXsDateTime());
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find(R"-("log_type":"bde")-") != std::string::npos);
    ASSERT_TRUE(output.find(startTimeStr) != std::string::npos);
}

TEST(BDEMessageTest, test1)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    testing::internal::CaptureStderr();

    const auto end = model::Timestamp::now() + model::Timestamp::duration_t(std::chrono::seconds(30));
    bdeMessage->update(BDEMessage::Data{.endTime = end});

    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find(R"-("log_type":"bde")-") != std::string::npos);
    ASSERT_TRUE(output.find(R"-("response_time":3000)-") !=
                std::string::npos);// NOTE last digit removed to avoid flaky test.
}

TEST(BDEMessageTest, loadProfiles)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    bdeMessage->update(BDEMessage::Data{.startTime = model::Timestamp(15.0),
                                        .endTime = model::Timestamp(25.0),
                                        .lastModified = model::Timestamp(10.0),
                                        .innerResponseCode = 123,
                                        .responseCode = 404});

    testing::internal::CaptureStderr();
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    TVLOG(1)<<output;
    ASSERT_TRUE(output.find("\"duration_in_ms\":5000") != std::string::npos);
    ASSERT_TRUE(output.find("\"response_code\":404") != std::string::npos);
    ASSERT_TRUE(output.find("\"response_time\":10000") != std::string::npos);
    ASSERT_TRUE(output.find("\"inner_response_code\":123") != std::string::npos);
}

TEST(BDEMessageTest, removePrefix)
{
    std::unique_ptr<BDEMessage> bdeMessage1 = std::make_unique<BDEMessage>();
    bdeMessage1->update(BDEMessage::Data{ .errorCode = "MEDICATIONSVC_abcde" });
    testing::internal::CaptureStderr();
    bdeMessage1 = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find("\"error_component\":\"abcde\"") != std::string::npos);

    std::unique_ptr<BDEMessage> bdeMessage2 = std::make_unique<BDEMessage>();
    bdeMessage2->update(BDEMessage::Data{ .errorCode = "MEDSVC_abcde" });
    testing::internal::CaptureStderr();
    bdeMessage2 = nullptr;
    output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find("\"error_component\":\"abcde\"") != std::string::npos);

    std::unique_ptr<BDEMessage> bdeMessage3 = std::make_unique<BDEMessage>();
    bdeMessage3->update(BDEMessage::Data{ .errorCode = "SOMETHING_abcde" });
    testing::internal::CaptureStderr();
    bdeMessage3 = nullptr;
    output = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(output.find("\"error_component\":\"SOMETHING_abcde\"") != std::string::npos);
}

TEST(BDEMessageTest, testMerge)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>(BDEMessage::Data{.startTime = model::Timestamp::now(), .hashedKvnr = model::HashedKvnr("bar"), .cid = "1234"}); // hashedKvnr is 626172h
    testing::internal::CaptureStderr();
    BDEMessage::Data delta;
    delta.hashedKvnr = model::HashedKvnr("foo"); // -> now it must be 666f6fh
    delta.endTime = model::Timestamp::now() + model::Timestamp::duration_t(std::chrono::seconds(30));
    delta.lastModified = model::Timestamp::now() + model::Timestamp::duration_t(std::chrono::seconds(10)); // lastModified timestamp is after startTime.
    bdeMessage->update(delta);
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    TVLOG(1) << output;
    ASSERT_TRUE(output.find("\"kvnr\":\"666f6f\"") != std::string::npos);
    ASSERT_TRUE(output.find("\"cid\":\"1234\"") != std::string::npos);
    ASSERT_TRUE(output.find("bde data mismatch") != std::string::npos);
}
