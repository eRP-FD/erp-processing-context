/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/Sha256.hxx"

#include <gtest/gtest.h>

namespace
{
class TestSample
{
public:
    std::string_view sample;
    std::string_view checksum;
};

std::ostream& operator << (std::ostream& out, const TestSample& testSample)
{
    out << R"({ "sample": ")" << testSample.sample << R"(", "checksum": ")" << testSample.checksum << R"(" })";
    return out;
}
}

class Sha256Test : public ::testing::TestWithParam<TestSample> {};

TEST_P(Sha256Test, samples)
{
    const auto& testSample = GetParam();
    EXPECT_EQ(Sha256::fromBin(testSample.sample), testSample.checksum);
}

using namespace std::string_view_literals;
INSTANTIATE_TEST_SUITE_P(samples, Sha256Test, ::testing::Values(
    TestSample{""sv                     , "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    TestSample{"Hello World!"sv         , "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069"},
    TestSample{"With Null\0 and One\1"sv, "66857f1962b84ca11cf693ca26ff2f97f167cda8b6205d84c00ca5d8af801887"}
));
