#include "erp/util/String.hxx"

#include "erp/util/Base64.hxx"

#include <gtest/gtest.h>
#include <map>


class StringTest : public testing::Test
{
public:
    static std::string vaListHelper(const char* msg, ...)
    {
        std::va_list args{};
        va_start(args, msg);
        return String::vaListToString(msg, args);
        va_end(args);
    }
};


TEST_F(StringTest, trim)
{
    ASSERT_EQ(String::trim("foo"), "foo");
    ASSERT_EQ(String::trim(" \tfoo "), "foo");
    ASSERT_EQ(String::trim("foo \t"), "foo");
    ASSERT_EQ(String::trim(" \tfoo"), "foo");

    ASSERT_EQ(String::trim(" \t\f\r\n"), "");
    ASSERT_EQ(String::trim(""), "");

    ASSERT_EQ(String::trim(" \tfoo bar\t "), "foo bar");
}


TEST_F(StringTest, split)
{
    ASSERT_EQ(String::split("", ';'), std::vector<std::string>({""}));
    ASSERT_EQ(String::split("a", ';'), std::vector<std::string>({"a"}));
    ASSERT_EQ(String::split("a;b", ';'), std::vector<std::string>({"a", "b"}));
    ASSERT_EQ(String::split("a;;b", ';'), std::vector<std::string>({"a", "", "b"}));
    ASSERT_EQ(String::split("a;b;c'd';e\"(;)\"f", ';'), std::vector<std::string>({"a", "b", "c'd'", "e\"(", ")\"f"}));
    ASSERT_EQ(String::split("a;", ';'), std::vector<std::string>({"a", ""}));
    ASSERT_EQ(String::split(";a", ';'), std::vector<std::string>({"", "a"}));
    ASSERT_EQ(String::split(";a;", ';'), std::vector<std::string>({"", "a", ""}));

    ASSERT_EQ(String::split("a-b-c", '-'), std::vector<std::string>({"a","b","c"}));
    ASSERT_EQ(String::split("a--c", '-'), std::vector<std::string>({"a","","c"}));
    ASSERT_EQ(String::split("-b-", '-'), std::vector<std::string>({"","b",""}));
    ASSERT_EQ(String::split("--c--", '-'), std::vector<std::string>({"","","c","",""}));
    ASSERT_EQ(String::split("a", '-'), std::vector<std::string>({"a"}));
    ASSERT_EQ(String::split("", '-'), std::vector<std::string>({""}));

    ASSERT_EQ(String::split(",token1 ,token2,token3,token4,token5,", ','),
        std::vector<std::string>({"", "token1 ", "token2", "token3", "token4", "token5", ""}));
}


TEST_F(StringTest, splitByStr)
{
    // ---- BEGIN EXPECTED SAME RESULTS AS WITH "StringTest.split" ABOVE ----
    ASSERT_EQ(String::split("", ";"), std::vector<std::string>({""}));
    ASSERT_EQ(String::split("a", ";"), std::vector<std::string>({"a"}));
    ASSERT_EQ(String::split("a;b", ";"), std::vector<std::string>({"a", "b"}));
    ASSERT_EQ(String::split("a;;b", ";"), std::vector<std::string>({"a", "", "b"}));
    ASSERT_EQ(String::split("a;b;c'd';e\"(;)\"f", ";"), std::vector<std::string>({"a", "b", "c'd'", "e\"(", ")\"f"}));
    ASSERT_EQ(String::split("a;", ";"), std::vector<std::string>({"a", ""}));
    ASSERT_EQ(String::split(";a", ";"), std::vector<std::string>({"", "a"}));
    ASSERT_EQ(String::split(";a;", ";"), std::vector<std::string>({"", "a", ""}));

    ASSERT_EQ(String::split("a-b-c", "-"), std::vector<std::string>({"a","b","c"}));
    ASSERT_EQ(String::split("a--c", "-"), std::vector<std::string>({"a","","c"}));
    ASSERT_EQ(String::split("-b-", "-"), std::vector<std::string>({"","b",""}));
    ASSERT_EQ(String::split("--c--", "-"), std::vector<std::string>({"","","c","",""}));
    ASSERT_EQ(String::split("a", "-"), std::vector<std::string>({"a"}));
    ASSERT_EQ(String::split("", "-"), std::vector<std::string>({""}));

    ASSERT_EQ(String::split(",token1 ,token2,token3,token4,token5,", ","),
        std::vector<std::string>({"", "token1 ", "token2", "token3", "token4", "token5", ""}));
    // ---- END EXPECTED SAME RESULTS AS WITH "StringTest.split" ABOVE ----

    ASSERT_EQ(String::split("", "--"), std::vector<std::string>({""}));
    ASSERT_EQ(String::split("a", "--"), std::vector<std::string>({"a"}));
    ASSERT_EQ(String::split("a--b", "--"), std::vector<std::string>({"a", "b"}));
    ASSERT_EQ(String::split("a----b", "--"), std::vector<std::string>({"a", "", "b"}));
    ASSERT_EQ(String::split("a--b--c'd'--e\"(--)\"f", "--"), std::vector<std::string>({"a", "b", "c'd'", "e\"(", ")\"f"}));
    ASSERT_EQ(String::split("a--", "--"), std::vector<std::string>({"a", ""}));
    ASSERT_EQ(String::split("--a", "--"), std::vector<std::string>({"", "a"}));
    ASSERT_EQ(String::split("--a--", "--"), std::vector<std::string>({"", "a", ""}));
    ASSERT_EQ(String::split("a---b", "--"), std::vector<std::string>({"a", "-b"}));

    ASSERT_EQ(String::split("HTTP/1.1 201 Created\r\nContent-Type: text\r\nContent-Length: 16\r\n\r\nthis is the body", "\r\n"),
        std::vector<std::string>({"HTTP/1.1 201 Created","Content-Type: text", "Content-Length: 16", "", "this is the body"}));
}


TEST_F(StringTest, splitWithStringQuoting)
{
    EXPECT_EQ(String::splitWithStringQuoting("", ';'), std::vector<std::string>({""}));
    EXPECT_EQ(String::splitWithStringQuoting("a", ';'), std::vector<std::string>({"a"}));
    EXPECT_EQ(String::splitWithStringQuoting("a;b", ';'), std::vector<std::string>({"a", "b"}));
    EXPECT_EQ(String::splitWithStringQuoting("a;;b", ';'), std::vector<std::string>({"a", "", "b"}));
    EXPECT_EQ(String::splitWithStringQuoting("a;b;c'd';e\"(;)\"f", ';'), std::vector<std::string>({"a", "b", "c'd'", "e\"(;)\"f"}));
    EXPECT_EQ(String::splitWithStringQuoting("a;", ';'), std::vector<std::string>({"a", ""}));
    EXPECT_EQ(String::splitWithStringQuoting(";a", ';'), std::vector<std::string>({"", "a"}));
    EXPECT_EQ(String::splitWithStringQuoting(";a;", ';'), std::vector<std::string>({"", "a", ""}));

    const std::string data = R"(,token1,"token2a,token2b",token3,'token4a,token4b',tok\,en5,)";
    EXPECT_EQ(String::splitWithStringQuoting(data, ','),
              std::vector<std::string>({"", "token1", "\"token2a,token2b\"", "token3", "'token4a,token4b'", "tok,en5", ""}));
}


TEST_F(StringTest, split_WithStringQuoting_epa_14050)
{
    std::string value = "MzAwMUB1dGltYWNvLWhzbS1zaW11bGF0b3IsMzAwMkB1dGltYWNvLWhzbS1zaW11bGF0b3I=";
    value = Base64::decodeToString(value);
    EXPECT_EQ(String::splitWithStringQuoting(value, ','), std::vector<std::string>({"3001@utimaco-hsm-simulator","3002@utimaco-hsm-simulator"}));
    EXPECT_EQ(String::split(value, ','), std::vector<std::string>({"3001@utimaco-hsm-simulator","3002@utimaco-hsm-simulator"}));
}


TEST_F(StringTest, splitWithStringQuoting_nestedQuoting)
{
    // This is from a "real-live" (early EVT test) example where the generating code seemed to miss
    // a closing quote and started to prepend double quotes with backslashes.
    ASSERT_EQ(String::splitWithStringQuoting("one;two=\"222\";three=\"four=\\\"bar\\\"\"", ';'), std::vector<std::string>({"one", "two=\"222\"", "three=\"four=\"bar\"\""}));
}


TEST_F(StringTest, splitIntoNullTerminatedArray)
{
    const auto textToSplit = " abc , def,ghi,";

    const auto [array,buffer] = String::splitIntoNullTerminatedArray(textToSplit, ",");

    ASSERT_EQ(array.size(), 5u);
    ASSERT_EQ(array[0], &buffer[0]);
    ASSERT_EQ(array[1], &buffer[4]);
    ASSERT_EQ(array[2], &buffer[8]);
    ASSERT_EQ(array[3], &buffer[12]);
    ASSERT_EQ(array[4], nullptr);
}


TEST_F(StringTest, concatenateStrings)
{
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({""}), ";"), std::string(""));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({""}), ";;;"), std::string(""));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a"}), ";"), std::string("a"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a"}), ";;;"), std::string("a"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "b"}), ";"), std::string("a;b"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "b"}), ";;;"), std::string("a;;;b"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "", "b"}), ";"), std::string("a;;b"));;
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "", "b"}), ";;;"), std::string("a;;;;;;b"));;

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"", "a", ""}), ";"), std::string(";a;"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"", "a", ""}), ";;;"), std::string(";;;a;;;"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({ "a","b","c" }), ";"), std::string("a;b;c"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({ "a","b","c" }), ";;;"), std::string("a;;;b;;;c"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "b", "c'd'", "e\"(", ")\"f"}), ";"), std::string("a;b;c'd';e\"(;)\"f"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"a", "b", "c'd'", "e\"(", ")\"f"}), ";;;"), std::string("a;;;b;;;c'd';;;e\"(;;;)\"f"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"","","c","",""}), ";"), std::string(";;c;;"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"","","c","",""}), ";;;"), std::string(";;;;;;c;;;;;;"));

    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"", "token1 ", "token2", "token3", "token4", "token5", ""}), ";"), std::string(";token1 ;token2;token3;token4;token5;"));
    ASSERT_EQ(String::concatenateStrings(std::vector<std::string>({"", "token1 ", "token2", "token3", "token4", "token5", ""}), ";;;"), std::string(";;;token1 ;;;token2;;;token3;;;token4;;;token5;;;"));
}


TEST_F(StringTest, concatenateItems)
{
    ASSERT_EQ(String::concatenateItems("a", std::string("b"), std::string_view("c")), std::string("abc"));
    ASSERT_EQ(String::concatenateItems(), std::string(""));
}


TEST_F(StringTest, removeTrailingChar)
{
    const std::string orig = "abcdefghijklmnopqrst";
    std::string result = orig;

    for(auto iter = orig.crbegin(); iter != orig.crend(); ++iter)
    {
        const auto expected = result.substr(0, result.size()-1);
        const auto removec = *iter;
        String::removeTrailingChar(result, removec);
        ASSERT_EQ(result, expected);
        String::removeTrailingChar(result, 'A');
        ASSERT_EQ(result, expected);
    }
}


TEST_F(StringTest, removeEnclosing_1)
{
    ASSERT_EQ(String::removeEnclosing("", "", ""), "");
    ASSERT_EQ(String::removeEnclosing("start", "end", ""), "");
    ASSERT_EQ(String::removeEnclosing("start", "end", "foo"), "foo");
    ASSERT_EQ(String::removeEnclosing("start", "end", "startfooend"), "foo");
    ASSERT_EQ(String::removeEnclosing("aaa", "aaa", "aaaaa"), "");
}


TEST_F(StringTest, removeEnclosing_2)
{
    std::string expected = "BLA";
    std::string head = "head";
    std::string tail = "tail";
    std::string orig = head + expected + tail;
    std::string result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    expected = "";

    orig = head + expected + tail;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    orig = "token";

    head = orig;
    tail = orig;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    head = "toke";
    tail = "oken";
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    head = "/*";
    tail = "*/";
    expected = "text in comments";
    orig = head + expected + tail;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    head = "/*";
    tail = "XX";
    expected = "text in comments*/";
    orig = head + expected;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    head = "XX";
    tail = "*/";
    expected = "/*text in comments";
    orig = expected + tail;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);

    orig = "/*text in comments*/";
    head = "";
    tail = "";
    expected = orig;
    result = String::removeEnclosing(head, tail, orig);
    ASSERT_EQ(result, expected);
}


TEST_F(StringTest, quoteNewlines)
{
    std::string orig = "\r\ntoken1\rtoken2\ntoken3\n";
    auto result = String::quoteNewlines(orig);
    ASSERT_EQ(result, "\\r\\n\ntoken1\\rtoken2\\n\ntoken3\\n");
}


TEST_F(StringTest, concatenateKeys)
{
    std::map<std::string, std::string> testMap = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
        {"key4", "value4"},
        {"key5", "value5"},
        {"key6", "value6"}
    };
    std::string result = String::concatenateKeys(testMap, "||");
    ASSERT_EQ(result, "key1||key2||key3||key4||key5||key6");

    testMap = { {"key1", "value1"} };
    result = String::concatenateKeys(testMap, "<split>");
    ASSERT_EQ(result, "key1");

    testMap = { };
    result = String::concatenateKeys(testMap, ",");
    ASSERT_EQ(result, "");

    std::map<int,int> testMap2 = {
            {1, 11}, {2, 12}, {3, 13}, {4, 4711}
    };
    result = String::concatenateKeys(testMap2, ";", [](int key) { return std::to_string(key); });
    ASSERT_EQ(result, "1;2;3;4");
}


TEST_F(StringTest, concatenateEntries)
{
    std::map<std::string, std::string> testMap = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
        {"key4", "value4"},
        {"key5", "value5"},
        {"key6", "value6"}
    };
    std::string result = String::concatenateEntries(testMap, "||", "--");
    ASSERT_EQ(result, "key1||value1--key2||value2--key3||value3--key4||value4--key5||value5--key6||value6");

    testMap = { {"key1", "value1"} };
    result = String::concatenateEntries(testMap, "||", "   ");
    ASSERT_EQ(result, "key1||value1");

    testMap = { };
    result = String::concatenateEntries(testMap, " ", ",");
    ASSERT_EQ(result, "");
}


TEST_F(StringTest, normalizeWs)
{
    std::string data = " \t\n    \rT\nE\tX   T\rE    \n\n\r ";
    auto result = String::normalizeWhitespace(data);
    ASSERT_EQ(result, "T E X T E");
}


TEST_F(StringTest, toUpper)
{
    ASSERT_EQ(String::toUpper("SESsion"), "SESSION");
}


TEST_F(StringTest, toLower)
{
    ASSERT_EQ(String::toLower("SESsion"), "session");
}


TEST_F(StringTest, toBool)
{
    ASSERT_EQ(String::toBool("TRUE"), true);
    ASSERT_EQ(String::toBool("true"), true);
    ASSERT_EQ(String::toBool("TrUe"), true);
    ASSERT_EQ(String::toBool("1"), true);

    ASSERT_EQ(String::toBool("FALSE"), false);
    ASSERT_EQ(String::toBool("false"), false);
    ASSERT_EQ(String::toBool("0"), false);
    ASSERT_EQ(String::toBool("something completely different"), false);
    ASSERT_EQ(String::toBool(""), false);
    ASSERT_EQ(String::toBool("   "), false);
}


TEST_F(StringTest, utf8Length)
{
    EXPECT_EQ(String::utf8Length(""), 0u);
    EXPECT_EQ(String::utf8Length("Abc Def Ghi"), 11u);
    EXPECT_EQ(String::utf8Length("ß"), 1u);
    EXPECT_EQ(String::utf8Length("🇩🇪"), 2u); // Two code points per flag: "D" "E"
}


TEST_F(StringTest, truncateUtf8)
{
    EXPECT_EQ(String::truncateUtf8("Easy", 4), "Easy");
    EXPECT_EQ(String::truncateUtf8("Easy", 2), "Ea");
    EXPECT_EQ(String::truncateUtf8("Easy", 0), "");

    EXPECT_EQ(String::truncateUtf8("ßßß", 4), "ßßß");
    EXPECT_EQ(String::truncateUtf8("Äää", 1), "Ä");
    EXPECT_EQ(String::truncateUtf8("ßßßß", 2), "ßß");

    // Test chopping off of trailing ASCII characters.
    EXPECT_EQ(String::truncateUtf8("ßAA", 2), "ßA");

    // Test with a four-byte character as well (truncate 𩸽𩸽 -> 𩸽).
    EXPECT_EQ(String::truncateUtf8("\xF0\xA9\xB8\xBD\xF0\xA9\xB8\xBD", 1), "\xF0\xA9\xB8\xBD");
}



TEST_F(StringTest, vaListToString)
{
    using namespace std::string_literals;
    EXPECT_EQ("hello world"s, vaListHelper("hello %s", "world"));
    EXPECT_EQ("hello world"s, vaListHelper("hello world"));
    EXPECT_EQ("hello 1"s, vaListHelper("hello %d", 1));
    EXPECT_EQ(""s, vaListHelper(""));
    EXPECT_EQ("hello world"s, vaListHelper("%s", "hello world"));

    // additional parameters are ignored:
    EXPECT_EQ("hello world"s, vaListHelper("hello world", "world"));
    EXPECT_EQ("hello world"s, vaListHelper("hello %s", "world", "hello"));

    EXPECT_ANY_THROW(vaListHelper(nullptr));
}
