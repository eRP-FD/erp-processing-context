/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/NumberAsStringParserDocument.hxx"

#include <gtest/gtest.h>

using model::NumberAsStringParserDocument;

// ---------------------------------------------------------------------------
// Helper: build a minimal JSON object with a single string field "v"
// and return the serialized result.
// ---------------------------------------------------------------------------
static std::string serializeWithValue(const std::string& value)
{
    // Build JSON inline so that makeString() (and thus sanitizeString()) is exercised.
    auto doc = NumberAsStringParserDocument::fromJson(R"({"v":"placeholder"})");
    doc.setValue(rapidjson::Pointer("/v"), value);
    return doc.serializeToJsonString();
}

// Convenience: parse and re-serialize a complete JSON string (exercises the
// String() parse handler path).
static std::string roundTrip(const std::string& json)
{
    return NumberAsStringParserDocument::fromJson(json).serializeToJsonString();
}

// ---------------------------------------------------------------------------
// Tests for the OWASP sanitization rules applied via makeString / setValue
// ---------------------------------------------------------------------------

class SanitizeStringTest : public ::testing::Test
{
};

// --- Rule: <script → \u003Cscript ---

TEST_F(SanitizeStringTest, scriptTagLowerCase)
{
    EXPECT_EQ(serializeWithValue("<script>alert(1)</script>"), R"({"v":"\u003Cscript>alert(1)\u003C/script>"})");
    // At minimum the opening < of <script must be escaped:
    EXPECT_NE(serializeWithValue("<script>alert(1)"), R"_({"v":"<script>alert(1)"})_");
}

// Case-insensitive matching is required by spec for <script and </script.

TEST_F(SanitizeStringTest, scriptTagUpperCase)
{
    EXPECT_EQ(serializeWithValue("<SCRIPT>alert(1)</SCRIPT>"), R"({"v":"\u003CSCRIPT>alert(1)\u003C/SCRIPT>"})");
}

TEST_F(SanitizeStringTest, scriptTagMixedCase)
{
    EXPECT_EQ(serializeWithValue("<Script>x</Script>"), R"({"v":"\u003CScript>x\u003C/Script>"})");
}

TEST_F(SanitizeStringTest, scriptTagAllVariants)
{
    // All four casing variants of <script in one string must all be escaped
    EXPECT_EQ(serializeWithValue("<script><SCRIPT><Script><sCrIpT>"),
              R"({"v":"\u003Cscript>\u003CSCRIPT>\u003CScript>\u003CsCrIpT>"})");
}

TEST_F(SanitizeStringTest, closingScriptTagUpperCase)
{
    EXPECT_EQ(serializeWithValue("foo</SCRIPT>bar"), R"({"v":"foo\u003C/SCRIPT>bar"})");
}

TEST_F(SanitizeStringTest, closingScriptTagMixedCase)
{
    EXPECT_EQ(serializeWithValue("foo</Script>bar"), R"({"v":"foo\u003C/Script>bar"})");
}

// --- Rule: </script → \u003C/script ---

TEST_F(SanitizeStringTest, closingScriptTag)
{
    const auto result = serializeWithValue("foo</script>bar");
    EXPECT_EQ(result, R"({"v":"foo\u003C/script>bar"})");
}

// --- Rule: <!-- → \u003C!-- ---

TEST_F(SanitizeStringTest, htmlComment)
{
    const auto result = serializeWithValue("<!--comment-->");
    EXPECT_EQ(result, R"({"v":"\u003C!--comment--\u003E"})");// --> rule fires on the --> inside the value
}

TEST_F(SanitizeStringTest, htmlCommentOnlyOpen)
{
    const auto result = serializeWithValue("<!--");
    EXPECT_EQ(result, R"({"v":"\u003C!--"})");
}

// --- Rule: ]]> → ]]\u003e ---

TEST_F(SanitizeStringTest, cdataClose)
{
    const auto result = serializeWithValue("]]>");
    EXPECT_EQ(result, R"({"v":"]]\u003E"})");
}

TEST_F(SanitizeStringTest, cdataCloseInContext)
{
    const auto result = serializeWithValue("<![CDATA[foo]]>bar");
    EXPECT_EQ(result, R"({"v":"<![CDATA[foo]]\u003Ebar"})");
}

// --- Rule: --> → --\u003E ---

TEST_F(SanitizeStringTest, arrowClose)
{
    const auto result = serializeWithValue("-->");
    EXPECT_EQ(result, R"({"v":"--\u003E"})");
}

TEST_F(SanitizeStringTest, arrowCloseInContext)
{
    const auto result = serializeWithValue("foo-->bar");
    EXPECT_EQ(result, R"({"v":"foo--\u003Ebar"})");
}

// --- Rule: U+2028 paragraph separator ---

TEST_F(SanitizeStringTest, paragraphSeparator)
{
    // UTF-8 encoding of U+2028 is 0xE2 0x80 0xA8
    const std::string input = "foo\xE2\x80\xA8"
                              "bar";
    const auto result = serializeWithValue(input);
    EXPECT_EQ(result, R"({"v":"foo\u2028bar"})");
}

// --- Rule: U+2029 line separator ---

TEST_F(SanitizeStringTest, lineSeparator)
{
    // UTF-8 encoding of U+2029 is 0xE2 0x80 0xA9
    const std::string input = "foo\xE2\x80\xA9"
                              "bar";
    const auto result = serializeWithValue(input);
    EXPECT_EQ(result, R"({"v":"foo\u2029bar"})");
}

// --- Multiple patterns in one value ---

TEST_F(SanitizeStringTest, multiplePatterns)
{
    // Value contains both a <script tag and U+2028
    const std::string input = "<script>\xE2\x80\xA8"
                              "alert()";
    const auto result = serializeWithValue(input);
    EXPECT_NE(result.find("\\u003C"), std::string::npos);
    EXPECT_NE(result.find("\\u2028"), std::string::npos);
}

// --- Repeated occurrences of the same pattern ---

TEST_F(SanitizeStringTest, multipleScriptOccurrences)
{
    const auto result = serializeWithValue("<script>a</script><script>b</script>");
    EXPECT_EQ(result, R"({"v":"\u003Cscript>a\u003C/script>\u003Cscript>b\u003C/script>"})");
}

// --- Clean value must not be modified ---

TEST_F(SanitizeStringTest, cleanValueUnchanged)
{
    const std::string clean = "Hello, World! 42 <3";
    // "<3" does not match "<script", "<!--", "</script" – must stay
    const auto result = serializeWithValue(clean);
    EXPECT_NE(result.find("<3"), std::string::npos);
}

TEST_F(SanitizeStringTest, emptyValue)
{
    const auto result = serializeWithValue("");
    EXPECT_EQ(result, R"({"v":""})");
}

// ---------------------------------------------------------------------------
// Regression: sanitization must also fire on the fromJson() parse path.
// If the document is constructed from incoming JSON the values must be
// sanitized just like those set via setValue().
// ---------------------------------------------------------------------------

class SanitizeParsePathTest : public ::testing::Test
{
};

// Sanitization now happens in serializeToJsonString(), which covers both
// the setValue() path and the fromJson() round-trip path.

TEST_F(SanitizeParsePathTest, fromJsonScriptTag)
{
    const auto result = roundTrip(R"({"v":"<script>alert(1)</script>"})");
    EXPECT_EQ(result, R"({"v":"\u003Cscript>alert(1)\u003C/script>"})");
}

TEST_F(SanitizeParsePathTest, fromJsonParagraphSeparator)
{
    // U+2028 inside a JSON string value (raw UTF-8 bytes in the JSON input)
    const std::string json = "{\"v\":\"foo\xE2\x80\xA8"
                             "bar\"}";
    const auto result = roundTrip(json);
    EXPECT_EQ(result, R"({"v":"foo\u2028bar"})");
}
