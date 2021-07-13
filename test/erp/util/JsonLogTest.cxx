#include "erp/util/JsonLog.hxx"

#include <gtest/gtest.h>


class JsonLogTest : public testing::Test
{
};


/**
 * This is not a test that is expected to fail but exists so that an observer can compare the output that is
 * made to cerr via TLOG(ERROR) or TVLOG(0) against the output that is expected in either logWithDetails
 * or logWithoutDetails.
 */
TEST_F(JsonLogTest, log)
{
    JsonLog log (LogId::IDP_UPDATE_FAILED);
    log.message("this is the log message")
        .details("only shown in debug builds");

    SUCCEED();
}


TEST_F(JsonLogTest, logWithDetails)
{
    std::ostringstream result;

    {
        JsonLog log (LogId::IDP_UPDATE_FAILED, result, true);
        log.message("this is the log message")
            .details("only shown in debug builds");
    }

    ASSERT_EQ(result.str(), R"({"id":285212673,"host":"127.0.0.1","port":9090,"message":"this is the log message","details":"only shown in debug builds"})");
}


TEST_F(JsonLogTest, logWithoutDetails)
{
    std::ostringstream result;

    {
        JsonLog log (LogId::IDP_UPDATE_FAILED, result, false);
        log.message("this is the log message")
            .details("only shown in debug builds");
    }

    ASSERT_EQ(result.str(), R"({"id":285212673,"host":"127.0.0.1","port":9090,"message":"this is the log message"})");
}


TEST_F(JsonLogTest, discard)
{
    std::ostringstream result;

    {
        JsonLog log (LogId::IDP_UPDATE_FAILED, result, false);
        log.message("this is the log message")
            .details("only shown in debug builds");
        log.discard();
    }

    ASSERT_EQ(result.str(), "");
}


TEST_F(JsonLogTest, keyValueDouble_1_234)
{
    std::ostringstream result;

    {
        JsonLog log (static_cast<LogId>(0), result, false);
        log.keyValue("key", 1.234);
    }

    ASSERT_EQ(result.str(), R"({"id":0,"host":"127.0.0.1","port":9090,"key":1.234})");
}


TEST_F(JsonLogTest, keyValueDouble_12345_6789)
{
    std::ostringstream result;

    {
        JsonLog log (static_cast<LogId>(0), result, false);
        log.keyValue("key", 12345.6789, 4);
    }

    ASSERT_EQ(result.str(), R"({"id":0,"host":"127.0.0.1","port":9090,"key":12345.6789})");
}


