/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/ModelException.hxx"
#include "shared/util/ErpException.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/Configuration.hxx"

#include <gtest/gtest.h>


class JsonLogTest : public testing::Test
{
};


class SomeModelException : public model::ModelException {
    using ModelException::ModelException;
};


/**
 * This is not a test that is expected to fail but exists so that an observer can compare the output that is
 * made to cerr via TLOG(ERROR) or TVLOG(0) against the output that is expected in either logWithDetails
 * or logWithoutDetails.
 */
TEST_F(JsonLogTest, log)
{
    JsonLog log (LogId::IDP_UPDATE_FAILED, JsonLog::makeVLogReceiver(0));
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

    std::stringstream expected;
    expected << R"({"info":"this is the log message","details":"only shown in debug builds"})";

    ASSERT_EQ(result.str(), expected.str());
}


TEST_F(JsonLogTest, logWithoutDetails)
{
    std::ostringstream result;

    {
        JsonLog log (LogId::IDP_UPDATE_FAILED, result, false);
        log.message("this is the log message")
            .details("only shown in debug builds");
    }

    std::stringstream expected;
    expected << R"({"info":"this is the log message"})";
    ASSERT_EQ(result.str(), expected.str());
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

    std::stringstream expected;
    expected << R"({"key":1.234})";
    ASSERT_EQ(result.str(), expected.str());
}


TEST_F(JsonLogTest, keyValueDouble_12345_6789)
{
    std::ostringstream result;

    {
        JsonLog log (static_cast<LogId>(0), result, false);
        log.keyValue("key", 12345.6789);
    }

    std::stringstream expected;
    expected << R"({"key":12345.6789})";
    ASSERT_EQ(result.str(), expected.str());
}

TEST_F(JsonLogTest, fromException)
{
    bool written = false;
    {
        JsonLog log{LogId::INTERNAL_WARNING, [&](std::string log){
            EXPECT_EQ(log, R"({"location":"unknown","exception":"std::runtime_error","what":"Oh No!","details":""})");
            written = true;
        }, true};
        log.valuesFromException(std::runtime_error{"Oh No!"});
    }
    EXPECT_TRUE(written);
}

TEST_F(JsonLogTest, fromWrapped)
{
    bool written = false;
    {
        JsonLog log{LogId::INTERNAL_WARNING, [&](std::string log){
            EXPECT_EQ(log, R"({"location":"here:1","exception":"ExceptionWrapper<std::runtime_error>","what":"Oh No!","details":""})");
            written = true;
        }, true};
        log.valuesFromException(ExceptionWrapper<std::runtime_error>::create({"here", 1}, "Oh No!"));
    }
    EXPECT_TRUE(written);
}

TEST_F(JsonLogTest, fromModelException)
{
    bool written = false;
    {
        JsonLog log{LogId::INTERNAL_WARNING, [&](std::string log){
            EXPECT_EQ(log, R"({"location":"here:1","exception":"ExceptionWrapper<SomeModelException>","details":""})");
            written = true;
        }, true};
        log.valuesFromException(ExceptionWrapper<SomeModelException>::create({"here", 1}, "Oh No!"));
    }
    EXPECT_TRUE(written);
}

TEST_F(JsonLogTest, fromErpExceptionNoDiag)
{
    bool written = false;
    {
        JsonLog log{LogId::INTERNAL_WARNING, [&](std::string log){
            EXPECT_EQ(log, R"({"location":"here:1","exception":"ExceptionWrapper<ErpException>","status":"BadRequest","what":"Oh No!","details":""})");
            written = true;
        }, true};
        log.valuesFromException(ExceptionWrapper<ErpException>::create({"here", 1}, "Oh No!", HttpStatus::BadRequest));
    }
    EXPECT_TRUE(written);
}

TEST_F(JsonLogTest, fromErpExceptionDiag)
{
    bool written = false;
    {
        JsonLog log{LogId::INTERNAL_WARNING, [&](std::string log){
            EXPECT_EQ(log, R"({"location":"here:1","exception":"ExceptionWrapper<ErpException>","status":"BadRequest","what":"Oh No!","details":"private info"})");
            written = true;
        }, true};
        log.valuesFromException(ExceptionWrapper<ErpException>::create({"here", 1}, "Oh No!", HttpStatus::BadRequest, "private info"));
    }
    EXPECT_TRUE(written);
}
