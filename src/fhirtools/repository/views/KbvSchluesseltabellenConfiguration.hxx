// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include <date/date.h>
#include <gsl/gsl-lite.hpp>
#include <rapidjson/document.h>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace fhirtools {
class FhirResourceGroupResolver;

class KbvSchluesseltabellenConfiguration {
public:
    struct Entry
    {
        std::string id;
        std::optional<date::local_days> start;
        std::optional<date::local_days> end;
        std::set<std::string> mGroups;
    };
    KbvSchluesseltabellenConfiguration() = default;
    KbvSchluesseltabellenConfiguration(const FhirResourceGroupResolver& resolver,
                                       const gsl::not_null<const rapidjson::Value*>& kbvSchluesseltabellen);

    void addEntry(Entry&& entry);

    void check() const;

    std::vector<Entry> entriesWithin(std::optional<date::local_days> start, std::optional<date::local_days> end) const;
    size_t size() const;

private:
    // sorted by end-date
    std::map<date::local_days, Entry> mEntries;
};

} // fhirtools
