/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/SafeString.hxx"

#include <gtest/gtest.h>

TEST(SafeStringTest, size)//NOLINT(readability-function-cognitive-complexity)
{
    {
        SafeString sf;
        ASSERT_EQ(sf.size(), 0u);
    }
    {
        SafeString sf(size_t(0));
        ASSERT_EQ(sf.size(), 0u);
    }
    {
        SafeString sf(123);
        ASSERT_EQ(sf.size(), 123u);
    }
    {
        std::string test_str = "test str";
        const auto test_str_size = test_str.size();
        SafeString sf(std::move(test_str));
        ASSERT_EQ(sf.size(), test_str_size);
        sf.safeErase();
        ASSERT_EQ(sf.size(), 0u);
    }
    {
        std::string test_str;
        const auto test_str_size = test_str.size();
        SafeString sf(std::move(test_str));
        ASSERT_EQ(sf.size(), test_str_size);
    }
}

TEST(SafeStringTest, TerminatingZero)//NOLINT(readability-function-cognitive-complexity)
{
    SafeString sf("hello");
    const auto size = sf.size();
    char* mutableView = sf;
    ASSERT_EQ(mutableView[size], '\0');

    mutableView[size] = 'x';    // illegally overwrite the \0
    ASSERT_EQ(sf.size(), size); // size should stay the same

    const char* pc1 = nullptr;
    char* pc2 = nullptr;
    gsl::span<const char> pc3;
    std::string_view strView;

    // The following assignments are expected to throw a runtime error (HEAP_CORRUPTION_DETECTED):
    EXPECT_THROW(pc1 = sf, std::runtime_error);
    ASSERT_EQ(pc1, nullptr); // Not really necessary. But avoids compiler warning "unused-but-set-variable".
    EXPECT_THROW(pc2 = sf, std::runtime_error);
    ASSERT_EQ(pc2, nullptr); // Not really necessary. But avoids compiler warning "unused-but-set-variable".
    EXPECT_THROW(pc3 = sf, std::runtime_error);
    EXPECT_THROW(strView = sf, std::runtime_error);
}

TEST(SafeStringTest, SafeErase)
{
    SafeString sf("hello");
    sf.safeErase();
    ASSERT_EQ(sf.size(), 0u);
    ASSERT_EQ(static_cast<const char*>(sf), nullptr); // not "" after safeErase
}

TEST(SafeStringTest, MoveConstructFromString)
{
    std::string theSource = "source string";
    const auto size = theSource.size();
    SafeString sf(std::move(theSource));
    ASSERT_EQ(theSource.size(), 0u); // NOLINT(bugprone-use-after-move, hicpp-invalid-access-moved)
    ASSERT_EQ(sf.size(), size);
    ASSERT_EQ(static_cast<std::string_view>(sf), "source string");
}

TEST(SafeStringTest, MoveAssignFromString)
{
    std::string theSource = "source string";
    const auto size = theSource.size();
    SafeString sf;
    sf = std::move(theSource);
    ASSERT_EQ(theSource.size(), 0u); // NOLINT(bugprone-use-after-move, hicpp-invalid-access-moved)
    ASSERT_EQ(sf.size(), size);
    ASSERT_EQ(static_cast<std::string_view>(sf), "source string");
}

TEST(SafeStringTest, MoveConstruct)
{
    SafeString sf1("source");
    const auto size = sf1.size();
    SafeString sf2(std::move(sf1));
    ASSERT_EQ(sf2.size(), size);
    ASSERT_EQ(static_cast<std::string_view>(sf2), "source");
    ASSERT_EQ(sf1.size(), 0u); // NOLINT(bugprone-use-after-move, hicpp-invalid-access-moved)
    ASSERT_EQ(static_cast<const char*>(sf1), nullptr);
}

TEST(SafeStringTest, DefaultConstructed)
{
    SafeString sf;
    ASSERT_EQ(sf.size(), 0u);
    ASSERT_EQ(static_cast<std::string_view>(sf), "");
}

TEST(SafeStringTest, SizeConstruced)
{
    {
        SafeString sf(1);
        ASSERT_EQ(sf.size(), 1u);
        const char* data = sf;
        ASSERT_EQ(data[sf.size()], '\0');
    }
    {
        SafeString sf(size_t(0));
        ASSERT_EQ(sf.size(), 0u);
        const char* data = sf;
        ASSERT_EQ(data[sf.size()], '\0');
    }
}

TEST(SafeStringTest, comparisonLength)//NOLINT(readability-function-cognitive-complexity)
{
    SafeString a1{"a1"};
    SafeString a11{"a11"};
    ASSERT_FALSE(a1 == a11);
    ASSERT_FALSE(a11 == a1);

    ASSERT_TRUE(a1 != a11);
    ASSERT_TRUE(a11 != a1);

    ASSERT_TRUE(a1 < a11);
    ASSERT_FALSE(a11 < a1);

    ASSERT_TRUE(a1 <= a11);
    ASSERT_FALSE(a11 <= a1);

    ASSERT_FALSE(a1 > a11);
    ASSERT_TRUE(a11 > a1);

    ASSERT_FALSE(a1 >= a11);
    ASSERT_TRUE(a11 >= a1);
}

TEST(SafeStringTest, comparisonLex)//NOLINT(readability-function-cognitive-complexity)
{
    SafeString a10{"a10"};
    SafeString a11{"a11"};

    ASSERT_FALSE(a10 == a11);
    ASSERT_FALSE(a11 == a10);

    ASSERT_TRUE(a10 != a11);
    ASSERT_TRUE(a11 != a10);

    ASSERT_TRUE(a10 < a11);
    ASSERT_FALSE(a11 < a10);

    ASSERT_TRUE(a10 <= a11);
    ASSERT_FALSE(a11 <= a10);

    ASSERT_FALSE(a10 > a11);
    ASSERT_TRUE(a11 > a10);

    ASSERT_FALSE(a10 >= a11);
    ASSERT_TRUE(a11 >= a10);
}

TEST(SafeStringTest, comparisonEqual)//NOLINT(readability-function-cognitive-complexity)
{
    SafeString a{"a10"};
    SafeString b{"a10"};

    ASSERT_TRUE(a == b);
    ASSERT_TRUE(b == a);

    ASSERT_FALSE(a != b);
    ASSERT_FALSE(b != a);

    ASSERT_FALSE(a < b);
    ASSERT_FALSE(b < a);

    ASSERT_TRUE(a <= b);
    ASSERT_TRUE(b <= a);

    ASSERT_FALSE(a > b);
    ASSERT_FALSE(b > a);

    ASSERT_TRUE(a >= b);
    ASSERT_TRUE(b >= a);
}

TEST(SafeStringTest, comparisonSelf)//NOLINT(readability-function-cognitive-complexity)
{
    SafeString a{"a10"};

    ASSERT_TRUE(a == a);
    ASSERT_FALSE(a != a);
    ASSERT_FALSE(a < a);
    ASSERT_TRUE(a <= a);
    ASSERT_FALSE(a > a);
    ASSERT_TRUE(a >= a);
}




