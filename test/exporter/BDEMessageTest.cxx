/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
* non-exclusively licensed to gematik GmbH
*/

#include "exporter/BdeMessage.hxx"

#include <gtest/gtest.h>


TEST(BDEMessageTest, loadProfiles)
{
    std::unique_ptr<BDEMessage> bdeMessage = std::make_unique<BDEMessage>();
    bdeMessage->mInnerResponseCode = 123;
    bdeMessage->mResponseCode = 404;
    bdeMessage->mStartTime = model::Timestamp(10.0);
    bdeMessage->mEndTime = model::Timestamp(20.0);
    bdeMessage->mLastModified = model::Timestamp(15.0);

    testing::internal::CaptureStderr();
    bdeMessage = nullptr;
    std::string output = testing::internal::GetCapturedStderr();
    TLOG(INFO) << output;
    ASSERT_TRUE(output.find("\"duration_in_ms\":18446744073709546616") != std::string::npos);
    ASSERT_TRUE(output.find("\"response_code\":404") != std::string::npos);
    ASSERT_TRUE(output.find("\"response_time\":10000") != std::string::npos);
    ASSERT_TRUE(output.find("\"inner_response_code\":123") != std::string::npos);
}

