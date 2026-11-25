#include "fhirtools/validator/internal/CodeChecker.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

#include <gtest/gtest.h>


class PrimitiveTypeTest : public testing::Test
{
public:
    void testCode(std::string_view code, std::optional<std::string_view> expectedMessage = std::nullopt)
    {
        auto result = fhirtools::CodeChecker::checkCode(code, "test");
        if (expectedMessage.has_value())
        {
            const auto& resultSet = result.results();
            ASSERT_EQ(resultSet.size(), 1);
            ASSERT_EQ(to_string(*resultSet.begin()), expectedMessage);
        }
        else
        {
            ASSERT_LE(result.highestSeverity(), fhirtools::Severity::warning) << (result.dumpToLog(), "see dump");
        }
    }
};

// http://hl7.org/fhir/R4/datatypes.html#primitive
// Technically, a code is restricted to a string which has at least one character and no leading
// or trailing whitespace, and where there is no whitespace other than single spaces in the contents
TEST_F(PrimitiveTypeTest, code_fail)
{
    // clang-format off
    EXPECT_NO_FATAL_FAILURE(testCode(" leading"      , "test: error: code must not start with whitespace."));
    EXPECT_NO_FATAL_FAILURE(testCode("trailing "     , "test: error: code must not end on whitespace."));
    EXPECT_NO_FATAL_FAILURE(testCode("Double  Space" , "test: error: code must not have more than one consecutive space."));
    EXPECT_NO_FATAL_FAILURE(testCode("Tab\tCharacter", "test: error: code must not contain whitespace other than space."));
    EXPECT_NO_FATAL_FAILURE(testCode("New\nLine"     , "test: error: code must not contain whitespace other than space."));
    EXPECT_NO_FATAL_FAILURE(testCode(" "             , "test: error: code must not start with whitespace."));

    EXPECT_NO_FATAL_FAILURE(testCode("\xe5What!"     , "test: error: invalid utf-8 in code."));
#define NBSP "\xC2\xA0"
    EXPECT_NO_FATAL_FAILURE(testCode("B" NBSP "E"    , "test: error: code must not contain whitespace other than space."));
#define EnSpace "\xE2\x80\x82"
    EXPECT_NO_FATAL_FAILURE(testCode("B" EnSpace "E" , "test: error: code must not contain whitespace other than space."));

}

TEST_F(PrimitiveTypeTest, code_valid)
{
    // clang-format off
    EXPECT_NO_FATAL_FAILURE(testCode("A"));                         // Minimum length
    EXPECT_NO_FATAL_FAILURE(testCode("Code\x20With\x20Space"));     // Single spaces (\x20)

    // UTF-8: "München" (ü = \xc3\xbc)
    EXPECT_NO_FATAL_FAILURE(testCode("M\xc3\xbcnchen"));

    // UTF-8: "你好" (Nǐ hǎo)
    EXPECT_NO_FATAL_FAILURE(testCode("\xe4\xbd\xa0\xe5\xa5\xbd"));    // clang-format on

}
