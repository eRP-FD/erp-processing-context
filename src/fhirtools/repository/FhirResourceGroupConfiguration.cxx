// (C) Copyright IBM Deutschland GmbH 2024
// (C) Copyright IBM Corp. 2024

#include "FhirResourceGroupConfiguration.hxx"
#include "fhirtools/FPExpect.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <ranges>
#include <regex>
#include <string>

namespace fhirtools
{

FhirResourceGroupConfiguration::~FhirResourceGroupConfiguration() = default;


class FhirResourceGroupConfiguration::Group final : public FhirResourceGroup, public std::enable_shared_from_this<Group>
{
public:
    Group(const FhirResourceGroupConfiguration& resolver, const rapidjson::Value& groupEntry);

    std::string_view id() const override;
    std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
    findVersion(const std::string& url) const override;
    std::optional<FhirVersion> findVersionLocal(const std::string& url) const override;

    bool addIfMatch(const std::string& url, const FhirVersion& version, const std::filesystem::path& sourceFile);

private:
    void parseMatch(const rapidjson::Value& matchEntry);
    void parseInclude(const FhirResourceGroupConfiguration& resolver, const rapidjson::Value& includeEntry);
    void insert(std::string url, FhirVersion version);

    struct Match {
        std::regex urlPattern;
        FhirVersion version;
    };

    std::string mId;
    std::list<Match> mMatches;
    std::list<std::regex> mFilePattern;
    std::set<std::shared_ptr<FhirResourceGroup>> mIncludes;
    std::unordered_map<std::string, FhirVersion> mVersionsByUrl;
};

FhirResourceGroupConfiguration::Group::Group(const FhirResourceGroupConfiguration& resolver,
                                             const rapidjson::Value& groupEntry)
{
    const rapidjson::Pointer ptEnvName{"/env-name"};
    const rapidjson::Pointer ptGroups{"/groups"};
    const rapidjson::Pointer ptId{"/id"};
    const rapidjson::Pointer ptInclude{"/include"};
    const rapidjson::Pointer ptMatch{"/match"};
    const rapidjson::Pointer ptValidFrom{"/valid-from"};
    const rapidjson::Pointer ptValidUntil{"/valid-until"};

    const auto* id = ptId.Get(groupEntry);
    FPExpect(id && id->IsString(), "/erp/fhir-resource-groups/id is mandatory and must be String");
    mId = id->GetString();
    VLOG(3) << "id: " << id->GetString() << ": ";
    const auto* matches = ptMatch.Get(groupEntry);
    FPExpect(matches && matches->IsArray(), "/erp/fhir-resource-groups/match is mandatory and must be an array");
    for (const auto& matchEntry : matches->GetArray())
    {
        parseMatch(matchEntry);
    }
    const auto* includes = ptInclude.Get(groupEntry);
    if (includes)
    {
        FPExpect(includes->IsArray(), "/erp/fhir-resource-groups/include is must be an array");
        for (const auto& includeEntry : includes->GetArray())
        {
            parseInclude(resolver, includeEntry);
        }
    }
}

std::string_view FhirResourceGroupConfiguration::Group::id() const
{
    return mId;
}

std::optional<FhirVersion> FhirResourceGroupConfiguration::Group::findVersionLocal(const std::string& url) const
{
    if (auto versionByUrl = mVersionsByUrl.find(url); versionByUrl != mVersionsByUrl.end())
    {
        return versionByUrl->second;
    }
    return std::nullopt;
}

bool FhirResourceGroupConfiguration::Group::addIfMatch(const std::string& url, const FhirVersion& version,
                                                       const std::filesystem::path& sourceFile)
{
    if (auto groupVersion = findVersionLocal(url); groupVersion == version)
    {
        return true;
    }
    for (const auto& match : mMatches)
    {
        if (match.version == version && regex_match(url, match.urlPattern))
        {
            insert(url, version);
            return true;
        }
    }
    for (const auto& pattern : mFilePattern)
    {
        if (regex_match(sourceFile.generic_string(), pattern))
        {
            insert(url, version);
            return true;
        }
    }
    return false;
}

std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
// NOLINTNEXTLINE(misc-no-recursion)
fhirtools::FhirResourceGroupConfiguration::Group::findVersion(const std::string& url) const
{
    std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>> result;
    result.first = findVersionLocal(url);
    result.second = result.first ? shared_from_this() : nullptr;
    for (const auto& incl : mIncludes)
    {
        if (auto inclResult = incl->findVersion(url); inclResult.first)
        {
            FPExpect3(! result.first.has_value() || inclResult == result, "Ambiguous version or group for: " + url,
                      std::logic_error);
            result = inclResult;
        }
    }
    return result;
}

void FhirResourceGroupConfiguration::Group::parseMatch(const rapidjson::Value& matchEntry)
{
    FPExpect(matchEntry.IsObject(), "match must be Object in group: " + mId);

    const rapidjson::Pointer ptUrl{"/url"};
    const rapidjson::Pointer ptVersion{"/version"};
    const rapidjson::Pointer ptFile{"/file"};

    const auto* url = ptUrl.Get(matchEntry);
    const auto* version = ptVersion.Get(matchEntry);
    const auto* file = ptFile.Get(matchEntry);
    FPExpect3((file && ! url && ! version) || (! file && url && version),
              "match must have either url and version or file", std::logic_error);
    FPExpect3(! url || url->IsString(), "match/url must be string in group: " + mId, std::logic_error);
    FPExpect3(! version || version->IsString() || version->IsNull(),
              "match/version must be string or null in group: " + mId, std::logic_error);
    FPExpect3(! file || file->IsString(), "match/file must be string in group: " + mId, std::logic_error);
    if (url && version)
    {
        mMatches.emplace_back(std::regex{url->GetString()},
                              version->IsNull() ? FhirVersion::notVersioned : FhirVersion{version->GetString()});
    }
    else
    {
        mFilePattern.emplace_back(file->GetString());
    }
}

void FhirResourceGroupConfiguration::Group::parseInclude(const FhirResourceGroupConfiguration& resolver,
                                                         const rapidjson::Value& includeEntry)
{
    FPExpect3(includeEntry.IsString(), "/erp/fhir-resource-groups/include must be an array of string",
              std::logic_error);
    std::string includedId = includeEntry.GetString();
    FPExpect(includedId != mId, "group includes itself: " + mId);
    if (auto includedGroup = resolver.mGroups.find(includedId); includedGroup != resolver.mGroups.end())
    {
        mIncludes.emplace(includedGroup->second);
    }
    else
    {
        FPFail("included group not found: " + includedId + " - make sure it is defined before the including group " +
               mId);
    }
}

void fhirtools::FhirResourceGroupConfiguration::Group::insert(std::string url, FhirVersion version)
{
    TVLOG(3) << "adding resource to group (" << mId << "): " << url << '|' << version;
    if (auto original = mVersionsByUrl.try_emplace(url, std::move(version)); ! original.second)
    {
        FPExpect3(original.first->second == version,
                  "Ambiguous versions (" + to_string(version) + ", " + to_string(original.first->second) +
                      ") in group (" + mId + "): " + url,
                  std::logic_error);
    }
}

FhirResourceGroupConfiguration::FhirResourceGroupConfiguration(
    const gsl::not_null<const rapidjson::Value*>& fhirResourceGroups)
{
    FPExpect(fhirResourceGroups->IsArray(), "/erp/fhir-resource-groups is not an array");
    for (const auto& groupEntry : fhirResourceGroups->GetArray())
    {
        auto group = std::make_shared<Group>(*this, groupEntry);
        std::string id{group->id()};
        TVLOG(2) << "Loaded group: " << id;
        mGroups.emplace(std::move(id), std::move(group));
    }
}

std::map<std::string, std::shared_ptr<const FhirResourceGroup>>
fhirtools::FhirResourceGroupConfiguration::allGroups() const
{
    std::map<std::string, std::shared_ptr<const FhirResourceGroup>> result;
    std::ranges::copy(mGroups, std::inserter(result, result.end()));
    return result;
}


std::shared_ptr<const FhirResourceGroup>
FhirResourceGroupConfiguration::findGroup(const std::string& url, const FhirVersion& version,
                                          const std::filesystem::path& sourceFile [[maybe_unused]]) const
{
    using namespace std::string_literals;
    std::shared_ptr<FhirResourceGroup> result;
    for (const auto& group : mGroups | std::views::values)
    {
        if (group->addIfMatch(url, version, sourceFile))
        {
            FPExpect(! result, "profile belongs to more than one group ("s.append(result->id())
                                   .append(", ")
                                   .append(group->id())
                                   .append("): ")
                                   .append(url)
                                   .append("|")
                                   .append(version.version().value_or("null")));
            result = group;
        }
    }
    return result;
}

std::shared_ptr<const FhirResourceGroup> FhirResourceGroupConfiguration::findGroupById(const std::string& id) const
{
    auto group = mGroups.find(id);
    if (group == mGroups.end())
    {
        return nullptr;
    }
    return group->second;
}

}// namespace fhirtools
