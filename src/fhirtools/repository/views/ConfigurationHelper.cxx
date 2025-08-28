// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "fhirtools/repository/views/ConfigurationHelper.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"


namespace fhirtools
{

std::string ConfigurationHelper::retrieveId(const rapidjson::Value& configItem)
{
    const rapidjson::Pointer ptId{"/id"};
    const auto* id = ptId.Get(configItem);
    FPExpect(id && id->IsString(), "fhir-resource-views/id is mandatory and must be String");
    return id->GetString();
}

std::optional<date::local_days> ConfigurationHelper::retrieveDate(const rapidjson::Pointer& pt,
                                                                  const rapidjson::Value& configItem)
{
    using namespace std::string_literals;
    if (const auto* configValue = pt.Get(configItem))
    {
        FPExpect((! configValue) || configValue->IsString(),
                 "resource-views/valid-* is optional, but must be String if provided");
        date::local_days day;
        std::istringstream actualStrm{configValue->GetString()};
        date::from_stream(actualStrm, "%Y-%m-%d", day);
        FPExpect3(! actualStrm.fail() && actualStrm.peek() == std::char_traits<char>::eof(),
                  "invalid date: "s.append(configValue->GetString()), std::logic_error);
        return day;
    }
    return std::nullopt;
}

std::set<std::string> ConfigurationHelper::retrieveGroups(const rapidjson::Value& configItem,
                                                          const FhirResourceGroupResolver& resolver,
                                                          const std::string& viewId)
{
    using namespace std::string_literals;
    const rapidjson::Pointer ptGroups{"/groups"};
    std::set<std::string> groups;
    const auto* groupIds = ptGroups.Get(configItem);
    FPExpect(groupIds && groupIds->IsArray(), "fhir-resource-views/groups is mandatory and must be Array");
    for (const auto& groupId : groupIds->GetArray())
    {
        FPExpect(groupId.IsString(), "/erp/fhir-resource-views/groups must be array of String");
        std::string groupS{groupId.GetString()};
        auto group = resolver.findGroupById(groupS);
        FPExpect(group != nullptr, "unknown group in view "s.append(viewId).append(": ").append(groupS));
        bool inserted = groups.emplace(groupS).second;
        FPExpect(inserted, "duplicate group in view "s.append(viewId).append(": ").append(groupS));
    }
    return groups;
}
}