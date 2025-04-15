/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirResourceGroup.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"

#include <rapidjson/pointer.h>
#include <ranges>

namespace fhirtools
{

fhirtools::FhirResourceViewConfiguration::ViewConfigInternal::ViewConfigInternal(std::string id, std::string envName,
                                                                                 std::optional<date::local_days> start,
                                                                                 std::optional<date::local_days> end,
                                                                                 std::set<std::string> groups)
    : ViewConfig{std::move(id), std::move(envName), start, end, std::move(groups)}
{
}

gsl::not_null<std::shared_ptr<FhirStructureRepository>> fhirtools::FhirResourceViewConfiguration::ViewConfig::view(
    gsl::not_null<const FhirStructureRepositoryBackend*> backend) const//NOLINT(performance-unnecessary-value-param)
{
    const auto& allGroups = backend->allGroups();
    std::set<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceGroup>>> groups;
    for (const auto& groupId : mGroups)
    {
        auto group = allGroups.find(groupId);
        FPExpect3(group != allGroups.end(), "group not found in backend: " + groupId, std::logic_error);
        groups.emplace(group->second);
    }
    return std::make_shared<fhirtools::FhirResourceViewGroupSet>(mId, std::move(groups), backend);
}

const FhirResourceViewConfiguration::ViewConfig& FhirResourceViewConfiguration::ViewList::latest() const
{
    const ViewConfig* result = nullptr;
    for (const auto& cfg : *this)
    {
        if (result == nullptr || cfg->mStart > result->mStart)
        {
            result = cfg.get().get();
        }
    }
    FPExpect3(result != nullptr, "FhirResourceViewConfiguration::ViewConfig::view called on empty list.",
              std::logic_error);
    return *result;
}

std::shared_ptr<FhirStructureRepository>
//NOLINTNEXTLINE(performance-unnecessary-value-param)
FhirResourceViewConfiguration::ViewList::match(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                               const std::string& url, FhirVersion version) const
{
    return match(std::move(backend), DefinitionKey{url, version});
}

std::shared_ptr<FhirStructureRepository>
//NOLINTNEXTLINE(performance-unnecessary-value-param)
FhirResourceViewConfiguration::ViewList::match(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                               const DefinitionKey& definitionKey) const
{
    std::shared_ptr<FhirStructureRepository> result;
    std::optional<date::local_days> resultStart;
    for (const auto& viewCfg : *this)
    {
        auto v = viewCfg->view(backend);
        if (v->findStructure(definitionKey) == nullptr)
        {
            continue;
        }
        if (! result || resultStart < viewCfg->mStart)
        {
            result = v;
            resultStart = viewCfg->mStart;
        }
    }
    return result;
}

FhirResourceViewConfiguration::ViewList
FhirResourceViewConfiguration::ViewList::matchAll(const FhirStructureRepositoryBackend& backend, const std::string& url,
                                                  FhirVersion version) const
{
    ViewList result;
    const DefinitionKey key{url, version};
    for (std::shared_ptr<const ViewConfig> viewCfg : *this)
    {
        auto v = viewCfg->view(&backend);
        if (v->findStructure(key) == nullptr)
        {
            continue;
        }

        result.emplace_back(viewCfg);
    }
    return result;
}

std::set<DefinitionKey> fhirtools::FhirResourceViewConfiguration::ViewList::supportedVersions(
    gsl::not_null<const FhirStructureRepositoryBackend*> backend,//NOLINT(performance-unnecessary-value-param)
    const std::list<std::string>& profileUrls) const
{
    std::set<DefinitionKey> result;
    for (const auto& viewCfg : *this)
    {
        auto v = viewCfg->view(backend);
        for (const auto& url : profileUrls)
        {
            const DefinitionKey key{url, std::nullopt};
            if (const auto* profile = v->findStructure(key))
            {
                result.emplace(profile->definitionKey());
            }
        }
    }
    return result;
}

FhirResourceViewConfiguration::FhirResourceViewConfiguration(
    const FhirResourceGroupResolver& resolver, const gsl::not_null<const rapidjson::Value*>& fhirResourceViews,
    const date::days& globalOffset)
{
    using namespace std::string_literals;
    FPExpect(fhirResourceViews->IsArray(), "fhir-resource-views is not an array");
    const rapidjson::Pointer ptId{"/id"};
    const rapidjson::Pointer ptEnvName{"/env-name"};
    const rapidjson::Pointer ptValidFrom{"/valid-from"};
    const rapidjson::Pointer ptValidUntil{"/valid-until"};
    const rapidjson::Pointer ptGroups{"/groups"};

    if (globalOffset != date::days{0})
    {
        TLOG(WARNING) << "adding " << globalOffset << " days GLOBAL OFFSET to all FHIR resource views";
    }

    for (const auto& item : fhirResourceViews->GetArray())
    {
        const auto* id = ptId.Get(item);
        FPExpect(id && id->IsString(), "fhir-resource-views/id is mandatory and must be String");

        const auto* envNameValue = ptEnvName.Get(item);
        FPExpect(envNameValue && envNameValue->IsString(), "fhir-resource-views/env-name is mandatory and must be String");
        std::string envName{envNameValue->GetString()};
        auto start = retrieveDateAndEnvVars(envName + "_VALID_FROM", ptValidFrom, item);
        auto end = retrieveDateAndEnvVars(envName + "_VALID_UNTIL", ptValidUntil, item);

        if (start && globalOffset != date::days{0})
        {
            *start += globalOffset;
        }
        if (end && globalOffset != date::days{0})
        {
            *end += globalOffset;
        }

        std::set<std::string> groups;
        const auto* groupIds = ptGroups.Get(item);
        FPExpect(groupIds && groupIds->IsArray(), "fhir-resource-views/groups is mandatory and must be Array");
        for (const auto& groupId : groupIds->GetArray())
        {
            FPExpect(groupId.IsString(), "/erp/fhir-resource-views/groups must be array of String");
            std::string groupS{groupId.GetString()};
            auto group = resolver.findGroupById(groupS);
            FPExpect(group != nullptr, "unknown group in view "s.append(envName).append(": ").append(groupS));
            bool inserted = groups.emplace(groupS).second;
            FPExpect(inserted, "duplicate group in view "s.append(envName).append(": ").append(groupS));
        }
        using namespace date::literals;
        TVLOG(1) << "view " << envName << " valid-from: " << start.value_or(date::local_days{})
                 << " valid-until: " << end.value_or(date::local_days{std::numeric_limits<std::chrono::days>::max()});
        mResourceViews.emplace(std::piecewise_construct, std::forward_as_tuple(end.value_or(date::local_days::max())),
                               std::forward_as_tuple(std::make_shared<ViewConfigInternal>(
                                   id->GetString(), envName, start, end, std::move(groups))));
    }
    check();
}

FhirResourceViewConfiguration::ViewList
FhirResourceViewConfiguration::getViewInfo(const date::local_days& validDate) const
{
    decltype(getViewInfo(validDate)) result;

    for (auto view = mResourceViews.lower_bound(validDate); view != mResourceViews.end() && view->first >= validDate;
         ++view)
    {
        if (! view->second->mStart || *view->second->mStart <= validDate)
        {
            result.emplace_back(view->second);
        }
    }
    return result;
}

FhirResourceViewConfiguration::ViewList FhirResourceViewConfiguration::getViewInfo(const Date& validDate) const
{
    return getViewInfo(date::local_days{date::year_month_day{validDate}});
}

std::list<gsl::not_null<std::shared_ptr<const FhirResourceViewConfiguration::ViewConfig>>>
FhirResourceViewConfiguration::allViews() const
{
    decltype(allViews()) result;
    std::ranges::transform(mResourceViews, std::back_inserter(result), [](const auto& v) {
        return v.second;
    });
    return result;
}

const std::vector<std::tuple<KeyData, std::string, std::string>>& FhirResourceViewConfiguration::variables() const
{
    return mVariables;
}

void FhirResourceViewConfiguration::check() const
{
    FPExpect3(! mResourceViews.empty(), "no view configured", std::logic_error);
    auto prev = mResourceViews.begin();
    FPExpect(prev->second->isConsistent(), (std::ostringstream{} << "inconsistent view: " << *prev->second).str());
    for (auto end = mResourceViews.end(), view = next(prev); view != end; prev = view, ++view)
    {
        FPExpect(view->second->isConsistent(), (std::ostringstream{} << "inconsistent view: " << view->second).str());
    }
}

std::optional<date::local_days> fhirtools::FhirResourceViewConfiguration::retrieveDateAndEnvVars(
    const std::string& envName, const rapidjson::Pointer& pt, const rapidjson::Value& configItem)
{
    const auto* configValue = pt.Get(configItem);
    FPExpect((! configValue) || configValue->IsString(),
             "resource-views/valid-* is optional, but must be String if provided");
    const auto defaultValue = configValue ? std::make_optional<std::string>(configValue->GetString()) : std::nullopt;

    KeyData keyData{.environmentVariable = envName, .jsonPath = ""};
    const auto envValue = Environment::get(keyData.environmentVariable);
    std::optional actual = envValue ? envValue : defaultValue;
    mVariables.emplace_back(keyData, actual.value_or("none"), defaultValue.value_or("none"));
    if (actual)
    {
        date::local_days day;
        std::istringstream actualStrm{envValue.value_or(*actual)};
        date::from_stream(actualStrm, "%Y-%m-%d", day);
        FPExpect3(! actualStrm.fail() && actualStrm.peek() == std::char_traits<char>::eof(), "invalid date: " + *actual,
                  std::logic_error);
        return day;
    }
    return std::nullopt;
}


bool FhirResourceViewConfiguration::ViewConfigInternal::isConsistent() const
{
    if (mStart && mEnd)
    {
        return *mStart <= *mEnd;
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const FhirResourceViewConfiguration::ViewConfig& view)
{
    os << view.mId;
    os << " Start: " << (view.mStart ? date::format("%Y-%m-%d", *view.mStart) : "none");
    os << " End: " << (view.mStart ? date::format("%Y-%m-%d", *view.mEnd) : "none");
    return os;
}

}
