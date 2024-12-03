#include "shared/util/Version.hxx"


#include <gtest/gtest.h>


class VersionTest : public ::testing::Test {
protected:
    void check(const Version& early, const Version& late)
    {
       EXPECT_NE(early, late);
       EXPECT_NE(late, early);

       EXPECT_LE(early, late);
       EXPECT_LT(early, late);
       EXPECT_NE(early, late);

       EXPECT_GE(late, early);
       EXPECT_GT(late, early);
       EXPECT_NE(late, early);

       EXPECT_FALSE(early == late);
       EXPECT_FALSE(late == early);
    }

};

TEST_F(VersionTest, compare)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace ::version_literal;
    EXPECT_EQ(""_ver, ""_ver);
    EXPECT_EQ("1"_ver, "1"_ver);
    EXPECT_EQ("a"_ver, "a"_ver);

    ASSERT_NO_FATAL_FAILURE(check("1"_ver, "2"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.0"_ver, "1.1"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1"_ver, "1.1"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1"_ver, "1.10"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.10"_ver, "1.10a"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1"_ver, "2"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1"_ver, "1a"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1a"_ver, "2.1a"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1-alpha"_ver, "1.1-beta"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1-beta"_ver, "1.1-rc1"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1-rc1"_ver, "1.1-rc2"_ver));
    ASSERT_NO_FATAL_FAILURE(check("1.1-rc1"_ver, "1.1"_ver));
}
