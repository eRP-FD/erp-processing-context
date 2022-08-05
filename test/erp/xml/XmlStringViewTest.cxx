#include "fhirtools/util/XmlHelper.hxx"

#include <gtest/gtest.h>

TEST(XmlStringViewTest, compare)
{
    using namespace xmlHelperLiterals;
    const xmlChar abc[] = "abc";
    const xmlChar abcd[] = "abcd";
    const auto svabc = "abc"_xs;
    const auto svabcd = "abcd"_xs;
    EXPECT_TRUE(abc == svabc);
    EXPECT_TRUE(svabc == abc);
    EXPECT_TRUE(abcd == svabcd);
    EXPECT_TRUE(svabcd == abcd);
    EXPECT_FALSE(abcd == svabc);
    EXPECT_FALSE(abc == svabcd);
    EXPECT_FALSE(svabcd == abc);
    EXPECT_FALSE(svabc == abcd);
}
