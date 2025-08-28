// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/util/MetricsRegistry.hxx"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class MetricsRegistryTest : public testing::Test
{
public:
    void SetUp() override
    {
        MetricsRegistry::instance().clear();
    }
    void TearDown() override
    {
        MetricsRegistry::instance().clear();
    }
};

TEST_F(MetricsRegistryTest, empty)
{
    EXPECT_TRUE(MetricsRegistry::instance().serialize().empty());
}

TEST_F(MetricsRegistryTest, single)
{
    EXPECT_NO_THROW(MetricsRegistry::instance().count(1ms, DurationCategory::postgres, "somemetric"));
    std::string serialized;
    ASSERT_NO_THROW(serialized = MetricsRegistry::instance().serialize());
    EXPECT_FALSE(serialized.empty());

std::string expected = R"(# HELP backend_duration_seconds Backend call duration in seconds
# TYPE backend_duration_seconds histogram
backend_duration_seconds_count{category="postgres",metric="somemetric"} 1
backend_duration_seconds_sum{category="postgres",metric="somemetric"} 0.001
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.001"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.005"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.01"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.05"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="+Inf"} 1
)";

    EXPECT_EQ(serialized, expected) << serialized;
}

TEST_F(MetricsRegistryTest, values)
{
    EXPECT_NO_THROW(MetricsRegistry::instance().count(1ms, DurationCategory::postgres, "somemetric"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(15ms, DurationCategory::postgres, "somemetric"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(11ms, DurationCategory::postgres, "somemetric"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(99999ms, DurationCategory::postgres, "somemetric"));
    std::string serialized;
    ASSERT_NO_THROW(serialized = MetricsRegistry::instance().serialize());
    EXPECT_FALSE(serialized.empty());

    std::string expected = R"(# HELP backend_duration_seconds Backend call duration in seconds
# TYPE backend_duration_seconds histogram
backend_duration_seconds_count{category="postgres",metric="somemetric"} 4
backend_duration_seconds_sum{category="postgres",metric="somemetric"} 100.026
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.001"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.005"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.01"} 1
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="0.05"} 3
backend_duration_seconds_bucket{category="postgres",metric="somemetric",le="+Inf"} 4
)";

    EXPECT_EQ(serialized, expected) << serialized;
}

TEST_F(MetricsRegistryTest, metrics)
{
    EXPECT_NO_THROW(MetricsRegistry::instance().count(1ms, DurationCategory::enrolment, "one"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(15ms, DurationCategory::enrolment, "two"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(11ms, DurationCategory::enrolment, "two"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(22ms, DurationCategory::enrolment, "one"));
    EXPECT_NO_THROW(MetricsRegistry::instance().count(33ms, DurationCategory::enrolment, "three"));
    std::string serialized;
    ASSERT_NO_THROW(serialized = MetricsRegistry::instance().serialize());
    EXPECT_FALSE(serialized.empty());

    std::string expected = R"(# HELP backend_duration_seconds Backend call duration in seconds
# TYPE backend_duration_seconds histogram
backend_duration_seconds_count{category="enrolment",metric="three"} 1
backend_duration_seconds_sum{category="enrolment",metric="three"} 0.033
backend_duration_seconds_bucket{category="enrolment",metric="three",le="0.001"} 0
backend_duration_seconds_bucket{category="enrolment",metric="three",le="0.005"} 0
backend_duration_seconds_bucket{category="enrolment",metric="three",le="0.01"} 0
backend_duration_seconds_bucket{category="enrolment",metric="three",le="0.05"} 1
backend_duration_seconds_bucket{category="enrolment",metric="three",le="+Inf"} 1
backend_duration_seconds_count{category="enrolment",metric="two"} 2
backend_duration_seconds_sum{category="enrolment",metric="two"} 0.026
backend_duration_seconds_bucket{category="enrolment",metric="two",le="0.001"} 0
backend_duration_seconds_bucket{category="enrolment",metric="two",le="0.005"} 0
backend_duration_seconds_bucket{category="enrolment",metric="two",le="0.01"} 0
backend_duration_seconds_bucket{category="enrolment",metric="two",le="0.05"} 2
backend_duration_seconds_bucket{category="enrolment",metric="two",le="+Inf"} 2
backend_duration_seconds_count{category="enrolment",metric="one"} 2
backend_duration_seconds_sum{category="enrolment",metric="one"} 0.023
backend_duration_seconds_bucket{category="enrolment",metric="one",le="0.001"} 1
backend_duration_seconds_bucket{category="enrolment",metric="one",le="0.005"} 1
backend_duration_seconds_bucket{category="enrolment",metric="one",le="0.01"} 1
backend_duration_seconds_bucket{category="enrolment",metric="one",le="0.05"} 2
backend_duration_seconds_bucket{category="enrolment",metric="one",le="+Inf"} 2
)";

    EXPECT_EQ(serialized, expected) << serialized;
}

