/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/message/MimeType.hxx"

#include "gtest/gtest.h"


void checkAcceptMimeType(std::string_view text, std::string_view expectedMimeType, float expectedWeight)
{
    const AcceptMimeType mimeType(text);
    ASSERT_EQ(mimeType.getMimeType(), expectedMimeType);
    ASSERT_FLOAT_EQ(mimeType.getQFactorWeight(), expectedWeight) << text;
}

void checkMimeType(std::string_view text, std::string_view expectedMimeType)
{
    const MimeType mimeType(text);
    ASSERT_EQ(mimeType.getMimeType(), expectedMimeType);
}

void checkContentMimeType(std::string_view text, std::string_view expectedMimeType, const std::optional<std::string>& expectedEncoding)
{
    const ContentMimeType mimeType(text);
    ASSERT_EQ(mimeType.getMimeType(), expectedMimeType);
    ASSERT_EQ(mimeType.getEncoding(), expectedEncoding) << text;
}

TEST(MimeTypeTest, valid)
{
    checkMimeType("text/html", "text/html");
    checkMimeType("application/xhtml+xml", "application/xhtml+xml");
    checkMimeType("application/xhtml+xml", "application/xhtml+xml");
    checkMimeType("application/xml;q=0.9", "application/xml");
    checkMimeType("*/*;q=0.123", "*/*");
    checkMimeType("x/y;q=1.0", "x/y");
    checkMimeType("  x/y  ;  q=1.0  ", "x/y");
    checkMimeType("  x/y  ;  q=1  ", "x/y");
    checkMimeType("x/y;z=a;q=0.5;a=b", "x/y");
    checkMimeType("x/y;z=a;q=0;a=b", "x/y");
    checkMimeType("x/y;z=a;q=0.0;a=b", "x/y");

    checkAcceptMimeType("text/html", "text/html", 1.0f);
    checkAcceptMimeType("application/xhtml+xml", "application/xhtml+xml", 1.0f);
    checkAcceptMimeType("application/xhtml+xml", "application/xhtml+xml", 1.0f);
    checkAcceptMimeType("application/xml;q=0.9", "application/xml", 0.9f);
    checkAcceptMimeType("*/*;q=0.123", "*/*", 0.123f);
    checkAcceptMimeType("x/y;q=1.0", "x/y", 1.0f);
    checkAcceptMimeType("  x/y  ;  q=1.0  ", "x/y", 1.0f);
    checkAcceptMimeType("  x/y  ;  q=1  ", "x/y", 1.0f);
    checkAcceptMimeType("x/y;z=a;q=0.5;a=b", "x/y", 0.5f);
    checkAcceptMimeType("x/y;z=a;q=0;a=b", "x/y", 0.f);
    checkAcceptMimeType("x/y;z=a;q=0.0;a=b", "x/y", 0.f);

    checkContentMimeType("application/fhir+json;charset=utf-8", "application/fhir+json", "utf-8");
    checkContentMimeType("application/fhir+XML;x=y;Charset=UTF-8;abc=123", "application/fhir+xml", "utf-8");
}

TEST(MimeTypeTest, invalid)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_ANY_THROW(MimeType(""));
    ASSERT_ANY_THROW(MimeType(","));
    ASSERT_ANY_THROW(MimeType("/"));
    ASSERT_ANY_THROW(MimeType(";"));
    ASSERT_ANY_THROW(MimeType("x"));
    ASSERT_ANY_THROW(MimeType("x/;"));
    ASSERT_ANY_THROW(MimeType("/y"));

    ASSERT_ANY_THROW(AcceptMimeType(""));
    ASSERT_ANY_THROW(AcceptMimeType(","));
    ASSERT_ANY_THROW(AcceptMimeType("/"));
    ASSERT_ANY_THROW(AcceptMimeType(";"));
    ASSERT_ANY_THROW(AcceptMimeType("x"));
    ASSERT_ANY_THROW(AcceptMimeType("x/;"));
    ASSERT_ANY_THROW(AcceptMimeType("x/y;q=1.1"));
    ASSERT_ANY_THROW(AcceptMimeType("x/y;q=-1.0"));
    ASSERT_ANY_THROW(AcceptMimeType("/y"));
}
