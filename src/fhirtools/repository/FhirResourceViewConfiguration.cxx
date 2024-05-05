/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "fhirtools/FPExpect.hxx"

#include <rapidjson/pointer.h>

namespace fhirtools
{

FhirResourceViewConfiguration::FhirResourceViewConfiguration(
    const gsl::not_null<const rapidjson::Value*>& fhirResourceTags,
    const gsl::not_null<const rapidjson::Value*>& fhirResourceViews)
{
    FPExpect(fhirResourceTags->IsArray(), "/erp/fhir-resource-tags is not an array");
    FPExpect(fhirResourceViews->IsArray(), "/erp/fhir-resource-views is not an array");
    const rapidjson::Pointer ptTag{"/tag"};
    const rapidjson::Pointer ptVersion{"/version"};
    const rapidjson::Pointer ptUrls{"/urls"};
    const rapidjson::Pointer ptEnvName{"/env-name"};
    const rapidjson::Pointer ptValidFrom{"/valid-from"};
    const rapidjson::Pointer ptValidUntil{"/valid-until"};
    const rapidjson::Pointer ptTags{"/tags"};

    std::unordered_multimap<std::string, DefinitionKey> tagContent;
    for (const auto& item : fhirResourceTags->GetArray())
    {
        const auto* tag = ptTag.Get(item);
        FPExpect(tag && tag->IsString(), "/erp/fhir-resource-tags/tag is mandatory and must be String");
        const auto* version = ptVersion.Get(item);
        FPExpect(version && version->IsString(), "/erp/fhir-resource-tags/tag is mandatory and must be String");
        const auto* urls = ptUrls.Get(item);
        FPExpect(urls && urls->IsArray(), "/erp/fhir-resource-tags/urls is mandatory and must be an array");
        for (const auto& url : urls->GetArray())
        {
            FPExpect(url.IsString(), "/erp/fhir-resource-tags/urls/url must be String");
            tagContent.emplace(tag->GetString(), DefinitionKey{url.GetString(), Version{version->GetString()}});
        }
    }

    for (const auto& item : fhirResourceViews->GetArray())
    {
        const auto* envName = ptEnvName.Get(item);
        FPExpect(envName && envName->IsString(), "/erp/fhir-resource-views/env-name is mandatory and must be String");

        auto start = retrieveDateAndEnvVars(std::string{envName->GetString()} + "_VALID_FROM", ptValidFrom, item);
        auto end = retrieveDateAndEnvVars(std::string{envName->GetString()} + "_VALID_UNTIL", ptValidUntil, item);

        const auto* tags = ptTags.Get(item);
        FPExpect(tags && tags->IsArray(), "/erp/fhir-resource-views/tags is mandatory and must be Array");
        std::list<ResourceView> mViews;
        for (const auto& tag : tags->GetArray())
        {
            FPExpect(tag.IsString(), "/erp/fhir-resource-views/tags/tag must be String");
            const std::string tagS{tag.GetString()};
            FPExpect(tagContent.contains(tag.GetString()),
                     "tag " + tagS + " used in view " + std::string{envName->GetString()} +
                         " not contained in /erp/fhir-resource-tags/ configuration.");
            const auto& definitionKeys = tagContent.equal_range(tagS);
            for (auto it = definitionKeys.first; it != definitionKeys.second; ++it)
            {
                insert(it->second.url, {.mStart = start, .mEnd = end, .mVersion = it->second.version});
            }
        }
    }

    check();
}

std::optional<std::string> FhirResourceViewConfiguration::getValidVersion(const std::string_view url,
                                                                          const Date& validDate) const
{
    auto candidate = mResourceViews.find(std::string{url});
    if (candidate != mResourceViews.end())
    {
        auto found = std::ranges::find_if(candidate->second, [&validDate](const auto& definition) {
            return definition.isValidAt(validDate);
        });
        return found != candidate->second.end() ? std::make_optional(found->mVersion) : std::nullopt;
    }
    return std::nullopt;
}

const std::vector<std::tuple<KeyData, std::string, std::string>>& FhirResourceViewConfiguration::variables() const
{
    return mVariables;
}

void FhirResourceViewConfiguration::check() const
{
    for (const auto& [u, resourceViews] : mResourceViews)
    {
        auto url = u;
        for (size_t i = 0; i < resourceViews.size(); ++i)
        {
            const auto& outer = resourceViews[i];
            FPExpect(outer.isConsistent(), (std::ostringstream{} << "inconsistent view: " << outer).str());
            for (size_t j = 0; j < resourceViews.size(); ++j)
            {
                const auto& inner = resourceViews[j];
                FPExpect(i == j || ! outer.overlaps(inner),
                         (std::ostringstream{}
                          << "fhir-resource-views must not overlap in validity. overlapping: " << url
                          << "|" << outer.mVersion << " and " << url << "|" << inner.mVersion)
                             .str());
            }
        }
    }
}

void FhirResourceViewConfiguration::insert(const std::string& url,
                                           const FhirResourceViewConfiguration::ResourceView& resourceView)
{
    auto candidate = mResourceViews.find(url);
    if (candidate == mResourceViews.end())
    {
        mResourceViews.emplace(url, std::vector{resourceView});
    }
    else
    {
        candidate->second.emplace_back(resourceView);
    }
}

std::optional<Date> FhirResourceViewConfiguration::retrieveDateAndEnvVars(const std::string_view envName,
                                                                          const rapidjson::Pointer& pt,
                                                                          const rapidjson::Value& configItem)
{
    const auto* configValue = pt.Get(configItem);
    FPExpect((! configValue) || configValue->IsString(),
             "/erp/fhir-resource-views/valid-* is optional, but must be String if provided");
    const auto defaultValue = configValue ? std::make_optional<std::string>(configValue->GetString()) : std::nullopt;

    if (defaultValue)
    {
        mEnvVars.emplace_back(envName);
        KeyData keyData{.environmentVariable = mEnvVars.back(), .jsonPath = ""};
        const auto envValue = Environment::get(keyData.environmentVariable.data());
        mVariables.emplace_back(keyData, envValue.value_or(*defaultValue), *defaultValue);

        return envValue ? std::make_optional<Date>(*envValue) : std::make_optional<Date>(*defaultValue);
    }
    return std::nullopt;
}

bool FhirResourceViewConfiguration::ResourceView::isValidAt(const fhirtools::Date& date) const
{
    if (mStart)
    {
        auto compareToStart = date.compareTo(*mStart);
        FPExpect(compareToStart,
                 (std::ostringstream{} << "could not compare date=" << date << " and mStart=" << *mStart).str());
        if (*compareToStart == std::strong_ordering::less)
        {
            return false;
        }
    }
    if (mEnd)
    {
        auto compareToEnd = date.compareTo(*mEnd);
        FPExpect(compareToEnd,
                 (std::ostringstream{} << "could not compare date=" << date << " and mEnd=" << *mEnd).str());
        if (*compareToEnd == std::strong_ordering::greater)
        {
            return false;
        }
    }
    return true;
}

bool FhirResourceViewConfiguration::ResourceView::overlaps(
    const FhirResourceViewConfiguration::ResourceView& other) const
{
    if ((! mStart && ! other.mStart) || (! mEnd && ! other.mEnd))
    {
        return true;
    }
    if (mStart && other.isValidAt(*mStart))
    {
        return true;
    }
    if (mEnd && other.isValidAt(*mEnd))
    {
        return true;
    }
    if (other.mStart && isValidAt(*other.mStart))
    {
        return true;
    }
    if (other.mEnd && isValidAt(*other.mEnd))
    {
        return true;
    }
    return false;
}

bool FhirResourceViewConfiguration::ResourceView::isConsistent() const
{
    if (mStart && mEnd)
    {
        return mStart->compareTo(*mEnd) != std::strong_ordering::greater;
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const FhirResourceViewConfiguration::ResourceView& view)
{
    os << "Start: " << view.mStart.value_or(Date{}) << " End: " << view.mEnd.value_or(Date{})
       << " Version: " << view.mVersion;
    return os;
}

}