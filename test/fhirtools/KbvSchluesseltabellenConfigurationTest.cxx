// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "fhirtools/repository/views/KbvSchluesseltabellenConfiguration.hxx"

#include <date/tz.h>
#include <gtest/gtest.h>

using namespace fhirtools;

class KbvSchluesseltabellenConfigurationTest : public testing::Test
{
public:
    KbvSchluesseltabellenConfigurationTest()
        : today(date::floor<date::days>(
              date::make_zoned("Europe/Berlin", std::chrono::system_clock::now()).get_local_time()))
        , tomorrow(today + date::days{1})
        , yesterday(today - date::days{1})
        , lastWeek(today - date::days{7})
        , nextWeek(today + date::days{7})
    {
    }

    date::local_days today;
    date::local_days tomorrow;
    date::local_days yesterday;
    date::local_days lastWeek;
    date::local_days nextWeek;
};

TEST_F(KbvSchluesseltabellenConfigurationTest, entriesWithin2)
{
    KbvSchluesseltabellenConfiguration config;
    config.addEntry({.id = "Q1", .start = std::nullopt, .end = today, .mGroups = {"group1"}});
    config.addEntry({.id = "Q2", .start = tomorrow, .end = std::nullopt, .mGroups = {"group2"}});

    {
        const auto entries = config.entriesWithin(std::nullopt, std::nullopt);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(lastWeek, yesterday);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, lastWeek);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, today);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, tomorrow);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(today, tomorrow);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(today, today);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(tomorrow, tomorrow);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(tomorrow, nextWeek);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(nextWeek, std::nullopt);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q2");
    }
}

TEST_F(KbvSchluesseltabellenConfigurationTest, entriesWithin3)
{
    KbvSchluesseltabellenConfiguration config;
    config.addEntry({.id = "Q1", .start = std::nullopt, .end = yesterday, .mGroups = {"group1"}});
    config.addEntry({.id = "Q2", .start = today, .end = tomorrow, .mGroups = {"group2"}});
    config.addEntry({.id = "Q3", .start = tomorrow + date::days{1}, .end = std::nullopt, .mGroups = {"group3"}});

    {
        const auto entries = config.entriesWithin(std::nullopt, std::nullopt);
        ASSERT_EQ(entries.size(), 3);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
        ASSERT_EQ(entries[2].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(lastWeek, yesterday);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, lastWeek);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q1");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, today);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, tomorrow);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(std::nullopt, nextWeek);
        ASSERT_EQ(entries.size(), 3);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
        ASSERT_EQ(entries[2].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(today, tomorrow);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(today, today);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q2");
    }

    {
        const auto entries = config.entriesWithin(nextWeek, nextWeek);
        ASSERT_EQ(entries.size(), 1);
        ASSERT_EQ(entries[0].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(tomorrow, nextWeek);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q2");
        ASSERT_EQ(entries[1].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(tomorrow, std::nullopt);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q2");
        ASSERT_EQ(entries[1].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(today, std::nullopt);
        ASSERT_EQ(entries.size(), 2);
        ASSERT_EQ(entries[0].id, "Q2");
        ASSERT_EQ(entries[1].id, "Q3");
    }

    {
        const auto entries = config.entriesWithin(yesterday, std::nullopt);
        ASSERT_EQ(entries.size(), 3);
        ASSERT_EQ(entries[0].id, "Q1");
        ASSERT_EQ(entries[1].id, "Q2");
        ASSERT_EQ(entries[2].id, "Q3");
    }
}
