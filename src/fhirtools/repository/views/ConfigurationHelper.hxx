// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include <date/date.h>
#include <rapidjson/pointer.h>
#include <optional>
#include <set>

namespace fhirtools
{
class FhirResourceGroupResolver;

class ConfigurationHelper
{
public:
    static std::string retrieveId(const rapidjson::Value& configItem);
    static std::optional<date::local_days> retrieveDate(const rapidjson::Pointer& pt,
                                                        const rapidjson::Value& configItem);
    static std::set<std::string> retrieveGroups(const rapidjson::Value& configItem,
                                                const FhirResourceGroupResolver& resolver, const std::string& viewId);
};

}// fhirtools
