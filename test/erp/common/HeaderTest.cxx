/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/common/Header.hxx"

#include <gtest/gtest.h>


class HeaderTest : public testing::Test
{
};


TEST_F(HeaderTest, EmptyHeaderFields)
{
    const Header header;

    ASSERT_TRUE(header.headers().empty());
}


TEST_F(HeaderTest, FilledHeaderFields)//NOLINT(readability-function-cognitive-complexity)
{
    Header::keyValueMap_t headerFields;
    headerFields.insert({"key1", "value1"});
    headerFields.insert({"key2", "value2"});

    Header header(
            HttpMethod::GET,
            std::string(),
            Header::Version_1_1,
            std::move(headerFields),
            HttpStatus::OK);

    ASSERT_FALSE(header.headers().empty());
    ASSERT_EQ(2u, header.headers().size());

    ASSERT_TRUE(header.hasHeader("key1"));
    ASSERT_EQ(header.header("key1").value(), "value1");
    ASSERT_TRUE(header.hasHeader("key2"));
    ASSERT_EQ(header.header("key2").value(), "value2");

    header.addHeaderField("key1", "othervalue");
    ASSERT_EQ(header.header("key1").value(), "othervalue");
}


TEST_F(HeaderTest, addRemoveHeaderField)
{
    Header header(
        HttpMethod::GET,
        std::string(),
        Header::Version_1_0,
        {},
        HttpStatus::OK);

    ASSERT_FALSE(header.hasHeader("key"));

    header.addHeaderField("key", "value");
    ASSERT_TRUE(header.hasHeader("key"));
    ASSERT_EQ(header.header("key").value(), "value");

    header.removeHeaderField("key");
    ASSERT_FALSE(header.hasHeader("key"));
}

namespace
{
    void checkInvariantsCommon(Header& header)//NOLINT(readability-function-cognitive-complexity)
    {
        ASSERT_NO_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::Continue);
        ASSERT_ANY_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::SwitchingProtocols);
        ASSERT_ANY_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::Processing);
        ASSERT_ANY_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::NoContent);
        ASSERT_ANY_THROW(header.checkInvariants());

        header.setMethod(HttpMethod::POST);
        ASSERT_NO_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::Continue);
        ASSERT_NO_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::SwitchingProtocols);
        ASSERT_NO_THROW(header.checkInvariants());
        header.setStatus(HttpStatus::Processing);
        ASSERT_NO_THROW(header.checkInvariants());
    }
}

TEST_F(HeaderTest, CheckInvariants1)
{
    Header header(
            HttpMethod::UNKNOWN,
            "/target/path/",
            Header::Version_1_1,
            { { Header::ContentLength, "15" } },
            HttpStatus::OK);

    checkInvariantsCommon(header);

    header.addHeaderField(Header::TransferEncoding, "chunked");
    ASSERT_ANY_THROW(header.checkInvariants());
}


TEST_F(HeaderTest, CheckInvariants2)
{
    Header header(
            HttpMethod::UNKNOWN,
            "/target/path/",
            Header::Version_1_1,
            { { Header::TransferEncoding, "chunked" } },
            HttpStatus::OK);

    checkInvariantsCommon(header);

    header.addHeaderField(Header::ContentLength, "22");
    ASSERT_ANY_THROW(header.checkInvariants());
}


TEST_F(HeaderTest, serialize)
{
    // Given
    Header header;
    header.addHeaderFields(
        std::unordered_map<std::string, std::string>({
            {"key1", "value1"},
            {"key2", "value2"}}));

    // When
    const std::string fields = header.serializeFields();

    // Then (order is undefined)
    ASSERT_TRUE((fields=="key1: value1\r\nkey2: value2\r\n") || (fields=="key2: value2\r\nkey1: value1\r\n"));
}

TEST_F(HeaderTest, acceptHeader)//NOLINT(readability-function-cognitive-complexity)
{
    Header header;
    header.addHeaderField(Header::Accept, "text/html, application/xhtml+xml, application/xml;q=0.9,*/*;q=0.8");
    ASSERT_TRUE(header.getAcceptMimeType("text/html"));
    ASSERT_FLOAT_EQ(header.getAcceptMimeType("text/html")->getQFactorWeight(), 1.0f);
    ASSERT_TRUE(header.getAcceptMimeType("application/xhtml+xml"));
    ASSERT_FLOAT_EQ(header.getAcceptMimeType("application/xhtml+xml")->getQFactorWeight(), 1.0f);
    ASSERT_TRUE(header.getAcceptMimeType("application/xml"));
    ASSERT_FLOAT_EQ(header.getAcceptMimeType("application/xml")->getQFactorWeight(), 0.9f);
    ASSERT_TRUE(header.getAcceptMimeType("*/*"));
    ASSERT_FLOAT_EQ(header.getAcceptMimeType("*/*")->getQFactorWeight(), 0.8f);
}


TEST_F(HeaderTest, moveConstructor)
{
    Header header;
    header.addHeaderField("test", "value");
    header.setStatus(HttpStatus::OK);

    Header header2 (std::move(header));
    EXPECT_FALSE(header.hasHeader("test"));//NOLINT[bugprone-use-after-move,hicpp-invalid-access-moved]

    EXPECT_EQ(header2.status(), HttpStatus::OK);
    ASSERT_TRUE(header2.hasHeader("test"));
    EXPECT_EQ(header2.header("test"), "value");
}

TEST_F(HeaderTest, keepAlive1_0)//NOLINT(readability-function-cognitive-complexity)
{
    Header header(HttpMethod::GET, "/", Header::Version_1_0, {}, HttpStatus::Unknown);

    {
        EXPECT_FALSE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_FALSE(connectionHeader.has_value());
    }

    header.setKeepAlive(false);
    {
        EXPECT_FALSE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_FALSE(connectionHeader.has_value());
    }

    header.setKeepAlive(true);
    {
        EXPECT_TRUE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_EQ(connectionHeader.value_or(""), Header::KeepAlive);
    }

    header.setKeepAlive(false);
    {
        EXPECT_FALSE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_EQ(connectionHeader.value_or(""), Header::ConnectionClose);
    }
}

TEST_F(HeaderTest, keepAlive1_1)//NOLINT(readability-function-cognitive-complexity)
{
    Header header(HttpMethod::GET, "/", Header::Version_1_1, {}, HttpStatus::Unknown);

    {
        EXPECT_TRUE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_FALSE(connectionHeader.has_value());
    }

    header.setKeepAlive(true);
    {
        EXPECT_TRUE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_FALSE(connectionHeader.has_value());
    }

    header.setKeepAlive(false);
    {
        EXPECT_FALSE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_EQ(connectionHeader.value_or(""), Header::ConnectionClose);
    }

    header.setKeepAlive(true);
    {
        EXPECT_TRUE(header.keepAlive());
        auto connectionHeader = header.header(Header::Connection);
        EXPECT_EQ(connectionHeader.value_or(""), Header::KeepAlive);
    }
}
