// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "fhirtools/repository/views/KbvSchluesseltabellenConfiguration.hxx"
#include "ConfigurationHelper.hxx"
#include "fhirtools/FPExpect.hxx"

#include <rapidjson/pointer.h>
#include <ranges>

namespace fhirtools
{

KbvSchluesseltabellenConfiguration::KbvSchluesseltabellenConfiguration(
    const FhirResourceGroupResolver& resolver, const gsl::not_null<const rapidjson::Value*>& kbvSchluesseltabellen)
{
    using namespace std::string_literals;
    FPExpect(kbvSchluesseltabellen->IsArray(), "schluesseltabellen is not an array");
    const rapidjson::Pointer ptId{"/id"};
    const rapidjson::Pointer ptValidFrom{"/valid-from"};
    const rapidjson::Pointer ptValidUntil{"/valid-until"};
    const rapidjson::Pointer ptGroups{"/groups"};

    for (const auto& item : kbvSchluesseltabellen->GetArray())
    {
        const auto id = ConfigurationHelper::retrieveId(item);
        auto start = ConfigurationHelper::retrieveDate(ptValidFrom, item);
        auto end = ConfigurationHelper::retrieveDate(ptValidUntil, item);
        std::set<std::string> groups = ConfigurationHelper::retrieveGroups(item, resolver, id);
        addEntry({.id = id, .start = start, .end = end, .mGroups = std::move(groups)});
    }

    check();
}

void KbvSchluesseltabellenConfiguration::addEntry(Entry&& entry)
{
    using namespace std::string_literals;
    const auto [it, inserted] = mEntries.try_emplace(entry.end.value_or(date::local_days::max()), std::move(entry));
    FPExpect(inserted, "could not add entry "s.append(entry.id));
}

void KbvSchluesseltabellenConfiguration::check() const
{
    using namespace std::string_literals;
    FPExpect(! mEntries.empty(), "no KBV Schluesseltabellen configured");
    const auto& [_, first] = *mEntries.begin();
    FPExpect(! first.start.has_value(), "first KBV Schluesseltabelle must not have start date.");
    const auto& [_2, last] = *mEntries.rbegin();
    FPExpect(! last.end.has_value(), "last KBV Schluesseltabelle must not have end date.");
    // check that the configuration has no time gaps and no overlaps
    for (auto it = mEntries.begin(); it != mEntries.end(); ++it)
    {
        if (it == mEntries.begin())
        {
            continue;
        }
        auto prev = std::prev(it);
        FPExpect(prev->second.end.has_value(),
                 "missing configured end date for KBV Schluesseltabelle "s.append(prev->second.id));
        FPExpect(it->second.start.has_value(),
                 "missing configured start date for KBV Schluesseltabelle "s.append(it->second.id));
        FPExpect(*it->second.start == *prev->second.end + date::days{1},
                 "end date of KBV Schluesseltabelle "s.append(prev->second.id)
                     .append(" must be one day before start date of KBV Schluesseltabelle ")
                     .append(it->second.id));
    }
}

std::vector<KbvSchluesseltabellenConfiguration::Entry>
KbvSchluesseltabellenConfiguration::entriesWithin(std::optional<date::local_days> start,
                                                  std::optional<date::local_days> end) const
{
    std::vector<Entry> result;
    const auto requestedStart = start.value_or(date::local_days::min());
    const auto requestedEnd = end.value_or(date::local_days::max());
    for (const auto& mEntry : mEntries | std::views::values)
    {
        const auto entryStart = mEntry.start.value_or(date::local_days::min());
        const auto entryEnd = mEntry.end.value_or(date::local_days::max());
        if (entryStart <= requestedEnd && requestedStart <= entryEnd)
        {
            result.emplace_back(mEntry);
        }
    }
    return result;
}

size_t KbvSchluesseltabellenConfiguration::size() const
{
    return mEntries.size();
}

}
