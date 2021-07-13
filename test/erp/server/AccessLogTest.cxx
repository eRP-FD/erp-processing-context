#include "erp/server/AccessLog.hxx"

#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"

#include <gtest/gtest.h>
#include <rapidjson/pointer.h>


class AccessLogTest : public testing::Test
{
public:

    static rapidjson::Document parseJsonLog (const std::string& json)
    {
        rapidjson::Document document;
        document.Parse(rapidjson::StringRef(json.data(), json.size()));
        Expect(!document.HasParseError(), "can not parse access log entry");
        return document;
    }

    static std::optional<std::string> getStringValue (const rapidjson::Document& document, const std::string& path)
    {
        const auto pointer = rapidjson::Pointer(rapidjson::StringRef(path.data(), path.size()));
        const auto* value = pointer.Get(document);
        if (value == nullptr)
            return std::nullopt;
        else
            return value->GetString();
    }

    static std::optional<size_t> getIntegerValue (const rapidjson::Document& document, const std::string& path)
    {
        const auto pointer = rapidjson::Pointer(rapidjson::StringRef(path.data(), path.size()));
        const auto* value = pointer.Get(document);
        if (value == nullptr)
            return std::nullopt;
        else
            return value->GetInt();
    }

    static std::optional<double> getDoubleValue (const rapidjson::Document& document, const std::string& path)
    {
        const auto pointer = rapidjson::Pointer(rapidjson::StringRef(path.data(), path.size()));
        const auto* value = pointer.Get(document);
        if (value == nullptr)
            return std::nullopt;
        else
            return value->GetDouble();
    }
};


TEST_F(AccessLogTest, logAfterConstruction)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    EXPECT_TRUE(getStringValue(document, "/timestamp").has_value());
    EXPECT_TRUE(getIntegerValue(document, "/port").has_value());

    EXPECT_TRUE(getDoubleValue(document, "/response-time").has_value());

    // x-request-id is not set by default as it is extracted from the outer request.
    EXPECT_FALSE(getStringValue(document, "/x-request-id").has_value());

    ASSERT_TRUE(getStringValue(document, "/log-type").has_value());
    EXPECT_EQ(getStringValue(document, "/log-type"), "access");

    // Verify that no "detail" has been added.
    ASSERT_FALSE(getStringValue(document, "/detail").has_value());
}


TEST_F(AccessLogTest, message)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.message("message content");
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getStringValue(document, "/message");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "message content");
}


TEST_F(AccessLogTest, error)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.error("error details");
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getStringValue(document, "/error");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "error details");
}


TEST_F(AccessLogTest, twoErrors)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.error("first");
        log.message("not an error");
        log.error("second");
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getStringValue(document, "/error");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "first;second");
}


TEST_F(AccessLogTest, setInnerRequestOperation)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.setInnerRequestOperation("GET /The/Operation");
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getStringValue(document, "/inner-request-operation");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "GET /The/Operation");
}


TEST_F(AccessLogTest, updateFromOuterRequest)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        auto outerRequest = ServerRequest(Header());
        outerRequest.header().addHeaderField("X-REQUEST-ID", "123");
        log.updateFromOuterRequest(outerRequest);
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getStringValue(document, "/x-request-id");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "123");
}


// updateFromInnerRequest does not yet do anything that could be tested


TEST_F(AccessLogTest, updateFromInnerResponse)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        auto innerResponse = ServerResponse(Header(HttpStatus::BadRequest), "");
        log.updateFromInnerResponse(innerResponse);
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getIntegerValue(document, "/inner-response-code");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), 400);
}


TEST_F(AccessLogTest, updateFromOuterResponse)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        auto outerResponse = ServerResponse(Header(HttpStatus::NoContent), "");
        log.updateFromOuterResponse(outerResponse);
        log.markEnd();
    }

    const auto document = parseJsonLog(os.str());
    const auto value = getIntegerValue(document, "/response-code");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), 204);
}


TEST_F(AccessLogTest, discard)
{
    std::ostringstream os;
    {
        AccessLog log (os);
        log.markEnd();
        log.discard();
    }

    ASSERT_TRUE(os.str().empty());
}

