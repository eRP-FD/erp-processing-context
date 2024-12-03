/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/UrlHelper.hxx"

#include "shared/util/ErpException.hxx"

#include <gtest/gtest.h>


using p1_t = std::pair<std::string, std::string>; // <input, expected output>
class UrlHelperTest1 : public testing::TestWithParam<p1_t>
{
};
TEST_P(UrlHelperTest1, removeTrailingSlash)
{
    auto result = UrlHelper::removeTrailingSlash(GetParam().first);
    ASSERT_EQ(result, GetParam().second);
}
INSTANTIATE_TEST_SUITE_P(removeTrailingSlashTests,
        UrlHelperTest1,
        testing::Values(
                p1_t{ "/", "" }, p1_t{ "//", "/" }, p1_t{ "some/string", "some/string" },
                p1_t{ "some/string/", "some/string" }, p1_t{ "", "" }));


using p2_t = std::pair<std::string, std::tuple<std::string, std::string, std::string>>; // <input, expected output>
class UrlHelperTest2 : public testing::TestWithParam<p2_t>
{
};
TEST_P(UrlHelperTest2, splitTarget)
{
    auto result = UrlHelper::splitTarget(GetParam().first);
    ASSERT_EQ(result, GetParam().second);
}
INSTANTIATE_TEST_SUITE_P(splitTargetTests, UrlHelperTest2,
        testing::Values(
                p2_t{ "index.html?p1=A&p2=B#ressource", { "index.html", "p1=A&p2=B", "ressource" }},
                p2_t{ "index.html#ressource", { "index.html", "", "ressource" }},
                p2_t{ "index.html#", { "index.html", "", "" }},
                p2_t{ "index.html?p1=A", { "index.html", "p1=A", "" }}, p2_t{ "x", { "x", "", "" }},
                p2_t{ "", { "", "", "" }}, p2_t{ "/Task/$create", { "/Task/$create", "", "" }}));

TEST(UrlHelperTest2, splitTargetInvalidUrl)
{
    ASSERT_ANY_THROW(UrlHelper::splitTarget("index.html#ressource?p1=A&p2=B"));
}


using p3_t = std::pair<std::string, std::unordered_map<std::string, std::string>>; // <input, expected output>
class UrlHelperTest3 : public testing::TestWithParam<p3_t>
{
};
TEST_P(UrlHelperTest3, slpitQuery)
{
    auto result = UrlHelper::splitQuery(GetParam().first);
    ASSERT_EQ(result.size(), GetParam().second.size());
    for (const auto& res : result)
    {
        ASSERT_GT(GetParam().second.count(res.first), 0u);
        ASSERT_EQ(res.second, GetParam().second.at(res.first));
    }
}
INSTANTIATE_TEST_SUITE_P(slpitQueryTests, UrlHelperTest3,
        testing::Values(
                p3_t{ "p1=A&p2=B", {{ "p1", "A" }, { "p2", "B" }}},
                p3_t{ "p1&p2=B", {{ "p1", "" }, { "p2", "B" }}},
                p3_t{ "p1=&p2=", {{ "p1", "" }, { "p2", "" }}},
                p3_t{ "", {}},
                p3_t{ "p1", {{ "p1", "" }}},
                p3_t{ "p1=", {{ "p1", "" }}},
                p3_t{ "p1=Huhu", {{ "p1", "Huhu" }}}));


class UrlHelperTest4 : public testing::TestWithParam<p1_t>
{
};
TEST_P(UrlHelperTest4, convertPathToRegex)
{
    auto result = UrlHelper::convertPathToRegex(GetParam().first);
    ASSERT_EQ(result, GetParam().second);
}
INSTANTIATE_TEST_SUITE_P(convertPathToRegexTests, UrlHelperTest4,
        testing::Values(
                p1_t{ "", "" },
                p1_t{ "https://max:muster@www.example.com:8080/index.html?p1=A&p2=B#ressource", "https://max:muster@www.example.com:8080/index.html?p1=A&p2=B#ressource" },
                p1_t{ "{a}", "([^/?]+)" },
                p1_t{ "{aa}", "([^/?]+)" },
                p1_t{ "{a}{a}", "([^/?]+)([^/?]+)" },
                p1_t{ "{}}", "{}}" },
                p1_t{ "{????}", "([^/?]+)" },
                p1_t{ "{x}$a", "([^/?]+)\\$a" },
                p1_t{ "$$", "\\$\\$" }));


using p4_t = std::pair<std::string, std::vector<std::string>>;
class UrlHelperTest5 :  public testing::TestWithParam<p4_t> {};
TEST_P(UrlHelperTest5, extractPathParameters)
{
    auto result = UrlHelper::extractPathParameters(GetParam().first);
    ASSERT_EQ(result.size(), GetParam().second.size());
    for (size_t i = 0u, end = result.size(); i < end; ++i)
    {
        ASSERT_EQ(result[i], GetParam().second[i]);
    }
}
INSTANTIATE_TEST_SUITE_P(extractPathParametersTests, UrlHelperTest5,
        testing::Values(
                p4_t{"{a}{bb}",{"a","bb"}},
                p4_t{"",{}},
                p4_t{"{}",{""}},
                p4_t{"{a}{a}",{"a","a"}},
                p4_t{"{}{}",{"",""}},
                p4_t{"{{}}",{"{"}},
                p4_t{"/Task/{id}/$activate",{"id"}}));

TEST(UrlHelperTest5, extractPathParametersBad)
{
    ASSERT_ANY_THROW(UrlHelper::extractPathParameters("{a}{"));
}


TEST(UrlHelperTest, unescapeUrl_success)
{
    EXPECT_EQ(UrlHelper::unescapeUrl("a+b"), "a b");
    EXPECT_EQ(UrlHelper::unescapeUrl("a%20b"), "a b");
    EXPECT_EQ(UrlHelper::unescapeUrl("%4a%4b%4c"), "JKL");
    EXPECT_EQ(UrlHelper::unescapeUrl("%4A%4B%4C"), "JKL");
    EXPECT_EQ(UrlHelper::unescapeUrl("http://bla.foo/path%2fwith%2fslashes"), "http://bla.foo/path/with/slashes");
}


TEST(UrlHelperTest, unescapeUrl_fail)//NOLINT(readability-function-cognitive-complexity)
{
    // Incomplete hex number.
    EXPECT_THROW(UrlHelper::unescapeUrl("a%2"), ErpException);

    // Invalid hex characters.
    EXPECT_ANY_THROW(UrlHelper::unescapeUrl("a%2x"));
}


TEST(UrlHelperTest, escapeUrl_success)
{
    EXPECT_EQ(UrlHelper::escapeUrl("a b"), "a%20b");

    EXPECT_EQ(UrlHelper::escapeUrl(std::string("a\000b", 3)), "a%00b");

    EXPECT_EQ(UrlHelper::escapeUrl("äöüÄÖÜß"), "%c3%a4%c3%b6%c3%bc%c3%84%c3%96%c3%9c%c3%9f");

    // Characters below 0x20 and above 0x7a are escaped.
    EXPECT_EQ(UrlHelper::escapeUrl("\037\040\041\177\200"), "%1f%20!%7f%80");

    // "$&+,/:;=?@\"<>#%{}|\\′~[]` " should be encoded.
    EXPECT_EQ(UrlHelper::escapeUrl("$&+,/:;=?@\"<>#%{}|\\^~[]` "),
              "%24%26%2b%2c%2f%3a%3b%3d%3f%40%22%3c%3e%23%25%7b%7d%7c%5c%5e%7e%5b%5d%60%20");
}


TEST(UrlHelperTest, escapeUrl_specialCharacters)
{
    // The set of special characters that is being escaped is work in progress.
    // Escaping is also dependent on position inside a URL (one ? is ok, but not more; '&' is ok to separate fields
    // but not inside paramter values).
    EXPECT_EQ(UrlHelper::escapeUrl("a%B"), "a%25B");
}

TEST(UrlHelperTest, parseSimpleDefaultHttpsPort)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("https://www.test123.de/somePath");

    EXPECT_EQ(urlParts.mProtocol, "https://");
    EXPECT_EQ(urlParts.mHost, "www.test123.de");
    EXPECT_EQ(urlParts.mPath, "/somePath");
    EXPECT_EQ(urlParts.mRest, "");
    EXPECT_EQ(urlParts.mPort, 443);
}

TEST(UrlHelperTest, parseExplicitHttpsPortAndQuery)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("https://www.test123.de:1000/somePath?andSome=params");

    EXPECT_EQ(urlParts.mProtocol, "https://");
    EXPECT_EQ(urlParts.mHost, "www.test123.de");
    EXPECT_EQ(urlParts.mPath, "/somePath");
    EXPECT_EQ(urlParts.mRest, "?andSome=params");
    EXPECT_EQ(urlParts.mPort, 1000);
}

TEST(UrlHelperTest, parseSimpleDefaultHttpPort)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("http://www.test123.de/anotherPath");

    EXPECT_EQ(urlParts.mProtocol, "http://");
    EXPECT_EQ(urlParts.mHost, "www.test123.de");
    EXPECT_EQ(urlParts.mPath, "/anotherPath");
    EXPECT_EQ(urlParts.mRest, "");
    EXPECT_EQ(urlParts.mPort, 80);
}

TEST(UrlHelperTest, parseSimpleDefaultFtpPort)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("ftp://localhost");

    EXPECT_EQ(urlParts.mProtocol, "ftp://");
    EXPECT_EQ(urlParts.mHost, "localhost");
    EXPECT_EQ(urlParts.mPath, "/");
    EXPECT_EQ(urlParts.mRest, "");
    EXPECT_EQ(urlParts.mPort, 80);
}

TEST(UrlHelperTest, parseMalformed)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("malformed");

    EXPECT_EQ(urlParts.mProtocol, "");
    EXPECT_EQ(urlParts.mHost, "malformed");
    EXPECT_EQ(urlParts.mPath, "/");
    EXPECT_EQ(urlParts.mRest, "");
    EXPECT_EQ(urlParts.mPort, 80);
}

TEST(UrlHelperTest, parseAndDropUserPart)
{
    // Here the user part is (currently) dropped
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("https://user@www.test123.de:90/somePath?andSome=params#andFragment");

    EXPECT_EQ(urlParts.mProtocol, "https://");
    EXPECT_EQ(urlParts.mHost, "www.test123.de");
    EXPECT_EQ(urlParts.mPath, "/somePath");
    EXPECT_EQ(urlParts.mRest, "?andSome=params#andFragment");
    EXPECT_EQ(urlParts.mPort, 90);
}

TEST(UrlHelperTest, parse_ERP6501_1)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("http://ocsp.ocsp-proxy.telematik/ocsp/http://qocsp-eA.medisign.de:8080/ocsp?param=something");

    EXPECT_EQ(urlParts.mProtocol, "http://");
    EXPECT_EQ(urlParts.mHost, "ocsp.ocsp-proxy.telematik");
    EXPECT_EQ(urlParts.mPath, "/ocsp/http://qocsp-eA.medisign.de:8080/ocsp");
    EXPECT_EQ(urlParts.mRest, "?param=something");
    EXPECT_EQ(urlParts.mPort, 80);
}

TEST(UrlHelperTest, parse_ERP6501_2)
{
    UrlHelper::UrlParts urlParts = UrlHelper::parseUrl("http://ocsp.ocsp-proxy.telematik/ocsp/http%3A%2F%2Fqocsp-eA.medisign.de%3A8080%2Focsp");

    EXPECT_EQ(urlParts.mProtocol, "http://");
    EXPECT_EQ(urlParts.mHost, "ocsp.ocsp-proxy.telematik");
    EXPECT_EQ(urlParts.mPath, "/ocsp/http%3A%2F%2Fqocsp-eA.medisign.de%3A8080%2Focsp");
    EXPECT_EQ(urlParts.mRest, "");
    EXPECT_EQ(urlParts.mPort, 80);
}
