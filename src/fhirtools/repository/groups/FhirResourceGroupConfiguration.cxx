// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirResourceGroupConfiguration.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/VersionMapper.hxx"

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
    using ConstGroupPtr = std::shared_ptr<const FhirResourceGroupConfiguration::Group>;
    Group(const FhirResourceGroupConfiguration& resolver, const rapidjson::Value& groupEntry,
          const std::shared_ptr<const VersionMapper>& versionMapper);

    std::string_view id() const override;
    std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
    findVersion(const DefinitionKey& key) const override;
    std::optional<FhirVersion> findVersionLocal(const DefinitionKey& key) const override;

    bool addIfMatch(const std::string& url, const FhirVersion& version, const std::filesystem::path& sourceFile, bool inferior);

    void logDependencyTree(size_t indent = 0) const;

private:
    struct ResourceInfo {
        FhirVersion version;
        bool inferior;
    };

    std::optional<ResourceInfo> findResourceInfoLocal(const DefinitionKey& key) const;
    std::pair<std::optional<ResourceInfo>, ConstGroupPtr> findResourceInfo(const DefinitionKey& key) const;


    void parseMatch(const rapidjson::Value& matchEntry);
    void parseInclude(const FhirResourceGroupConfiguration& resolver, const rapidjson::Value& includeEntry);
    void parseExtend(const FhirResourceGroupConfiguration& resolver, const rapidjson::Value& extendEntry);
    void insert(std::string url, FhirVersion version, bool inferior);
    void findResourceInfoIn(std::pair<std::optional<ResourceInfo>, ConstGroupPtr>& result, const DefinitionKey& key,
                       const std::set<ConstGroupPtr>& groups) const;

    struct Match {
        std::regex urlPattern;
        FhirVersion version;
    };

    std::string mId;
    std::list<Match> mMatches;
    std::list<std::regex> mFilePattern;
    std::set<ConstGroupPtr> mIncludes;
    std::set<ConstGroupPtr> mExtends;
    std::shared_ptr<const VersionMapper> mVersionMapper;


    std::unordered_map<std::string, ResourceInfo> mResourceInfoByUrl;
};

FhirResourceGroupConfiguration::Group::Group(const FhirResourceGroupConfiguration& resolver,
                                             const rapidjson::Value& groupEntry,
                                             const std::shared_ptr<const VersionMapper>& versionMapper)
{
    const rapidjson::Pointer ptId{"/id"};
    const rapidjson::Pointer ptInclude{"/include"};
    const rapidjson::Pointer ptExtend{"/extend"};
    const rapidjson::Pointer ptMatch{"/match"};
    const rapidjson::Pointer ptMapVersions{"/map-versions"};

    const auto* id = ptId.Get(groupEntry);
    FPExpect(id && id->IsString(), "/fhir-resource-groups/id is mandatory and must be String");
    mId = id->GetString();
    VLOG(3) << "id: " << id->GetString() << ": ";
    const auto* matches = ptMatch.Get(groupEntry);
    FPExpect(matches && matches->IsArray(), "/fhir-resource-groups/match is mandatory and must be an array");
    for (const auto& matchEntry : matches->GetArray())
    {
        parseMatch(matchEntry);
    }
    const auto* includes = ptInclude.Get(groupEntry);
    if (includes)
    {
        FPExpect(includes->IsArray(), "/fhir-resource-groups/include must be an array");
        for (const auto& includeEntry : includes->GetArray())
        {
            parseInclude(resolver, includeEntry);
        }
    }
    const auto* extends = ptExtend.Get(groupEntry);
    if (extends)
    {
        FPExpect(extends->IsArray(), "/fhir-resource-groups/extend must be an array");
        for (const auto& extendEntry : extends->GetArray())
        {
            parseExtend(resolver, extendEntry);
        }
    }
    if (const auto* mapVersions = ptMapVersions.Get(groupEntry))
    {
        FPExpect(mapVersions->IsBool(), "/fhir-resource-groups/map-versions must be boolean");
        if (mapVersions->GetBool())
        {
            FPExpect3(versionMapper != nullptr, "group has map-versions but versionMapper is nullptr: " + mId,
                      std::logic_error);
            mVersionMapper = versionMapper;
        }
    }

}

std::string_view FhirResourceGroupConfiguration::Group::id() const
{
    return mId;
}

std::optional<FhirVersion>
FhirResourceGroupConfiguration::Group::findVersionLocal(const DefinitionKey& key) const
{
    auto res = findResourceInfoLocal(key);
    return res?std::optional{res->version}:std::nullopt;
}

std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
fhirtools::FhirResourceGroupConfiguration::Group::findVersion(const DefinitionKey& key) const
{
    auto res = findResourceInfo(key);
    return {res.first?std::optional{res.first->version}:std::nullopt, std::move(res.second)};
}


std::optional<FhirResourceGroupConfiguration::Group::ResourceInfo>
FhirResourceGroupConfiguration::Group::findResourceInfoLocal(const DefinitionKey& key) const
{
    auto unmappedOK = [&](const FhirVersion& foundVersion) {
        return (! key.version || *key.version == foundVersion);
    };
    auto mappedOK = [&](const FhirVersion& foundVersion) {
        return (mVersionMapper && mVersionMapper->findMapping({key.url, foundVersion}, key) != nullptr);
    };
    if (auto resourceInfoByUrl = mResourceInfoByUrl.find(key.url);
        resourceInfoByUrl != mResourceInfoByUrl.end() &&
        (unmappedOK(resourceInfoByUrl->second.version) || mappedOK(resourceInfoByUrl->second.version)))
    {
        return resourceInfoByUrl->second;
    }
    return std::nullopt;
}

bool FhirResourceGroupConfiguration::Group::addIfMatch(const std::string& url, const FhirVersion& version,
                                                       const std::filesystem::path& sourceFile, bool inferior)
{
    if (auto groupVersion = findVersionLocal({url, version}); groupVersion == version)
    {
        return true;
    }
    for (const auto& match : mMatches)
    {
        if (match.version == version && regex_match(url, match.urlPattern))
        {
            insert(url, version, inferior);
            return true;
        }
    }
    for (const auto& pattern : mFilePattern)
    {
        if (regex_match(sourceFile.generic_string(), pattern))
        {
            insert(url, version, inferior);
            return true;
        }
    }
    return false;
}

//NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::FhirResourceGroupConfiguration::Group::logDependencyTree(size_t indent) const
{
    TVLOG(1) << std::right << std::setw(static_cast<int>(indent)) << ' ' << mId;
    if (! mIncludes.empty())
    {
        TVLOG(1) << std::right << std::setw(static_cast<int>(indent + 2)) << ' ' << "include:";
        for (const auto& g : mIncludes)
        {
            static_pointer_cast<const FhirResourceGroupConfiguration::Group>(g)->logDependencyTree(indent + 4);
        }
    }
    if (! mExtends.empty())
    {
        TVLOG(1) << std::right << std::setw(static_cast<int>(indent + 2)) << ' ' << "extend:";
        for (const auto& g : mExtends)
        {
            static_pointer_cast<const FhirResourceGroupConfiguration::Group>(g)->logDependencyTree(indent + 4);
        }
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
auto FhirResourceGroupConfiguration::Group::findResourceInfo(const DefinitionKey& key) const
    -> std::pair<std::optional<ResourceInfo>, ConstGroupPtr>
{
    std::pair<std::optional<ResourceInfo>, ConstGroupPtr> result;
    result.first = findResourceInfoLocal(key);
    result.second = result.first ? shared_from_this() : nullptr;
    findResourceInfoIn(result, key, mIncludes);
    if (result.first && ! result.first->inferior)
    {
        return result;
    }
    findResourceInfoIn(result, key, mExtends);
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::FhirResourceGroupConfiguration::Group::findResourceInfoIn(
    std::pair<std::optional<ResourceInfo>, ConstGroupPtr>& result, const DefinitionKey& key,
    const std::set<ConstGroupPtr>& groups) const
{
    using namespace std::string_literals;
    std::vector<std::string> collectedErrors;
    for (const auto& grp : groups)
    {
        if (auto grpResult = grp->findResourceInfo(key); grpResult.first)
        {
            if (! result.first.has_value() || (result.first->inferior && !grpResult.first->inferior))
            {
                result = grpResult;
            }
            else if (! grpResult.first->inferior && ! result.first->inferior &&
                     (grpResult.first->version != result.first->version || grpResult.second != result.second))
            {
                collectedErrors.emplace_back("In group " + mId + ": ambiguous version ("s + to_string(value(result.first).version) + " vs. "s +
                            to_string(value(grpResult.first).version) + ") or group ("s.append(result.second->id()) +
                            " vs. "s.append(grpResult.second->id()) + ") for: "s + to_string(key));
            }
        }
    }
    if (!collectedErrors.empty())
    {
        logDependencyTree();
        for (const auto & collectedError : collectedErrors)
        {
            LOG(ERROR) << collectedError;
        }
        FPFail2("see errors above", std::logic_error);
    }
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

void FhirResourceGroupConfiguration::Group::parseExtend(const FhirResourceGroupConfiguration& resolver,
                                                        const rapidjson::Value& extendEntry)
{
    FPExpect3(extendEntry.IsString(), "/erp/fhir-resource-groups/extend must be an array of string", std::logic_error);
    std::string extendedId = extendEntry.GetString();
    FPExpect(extendedId != mId, "group extends itself: " + mId);
    if (auto extendedGroup = resolver.mGroups.find(extendedId); extendedGroup != resolver.mGroups.end())
    {
        FPExpect(! mIncludes.contains(extendedGroup->second),
                 "Group " + mId + " must not extend and include the same group: " + extendedId);
        mExtends.emplace(extendedGroup->second);
    }
    else
    {
        FPFail("extended group not found: " + extendedId + " - make sure it is defined before the extending group " +
               mId);
    }
}


void fhirtools::FhirResourceGroupConfiguration::Group::insert(std::string url, FhirVersion version, bool inferior)
{
    TVLOG(3) << "adding resource to group (" << mId << "): " << url << '|' << version;
    if (auto original = mResourceInfoByUrl.try_emplace(url, std::move(version), inferior); ! original.second)
    {
        FPExpect3(original.first->second.version == version,
                  "Ambiguous versions (" + to_string(version) + ", " + to_string(original.first->second.version) +
                      ") in group (" + mId + "): " + url,
                  std::logic_error);
    }
}

FhirResourceGroupConfiguration::FhirResourceGroupConfiguration(
    const gsl::not_null<const rapidjson::Value*>& fhirResourceGroups, const std::shared_ptr<const VersionMapper>& versionMapper)
{
    FPExpect(fhirResourceGroups->IsArray(), "/erp/fhir-resource-groups is not an array");
    for (const auto& groupEntry : fhirResourceGroups->GetArray())
    {
        auto group = std::make_shared<Group>(*this, groupEntry, versionMapper);
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
fhirtools::FhirResourceGroupConfiguration::findGroup(const std::string& url, const FhirVersion& version,
                                                     const std::filesystem::path& sourceFile) const
{
    return findGroupInternal(url, version, sourceFile, false);
}

std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupConfiguration::findGroup(const FhirValueSet& valueSet) const
{
    return findGroup(valueSet.getUrl(), valueSet.getVersion(), valueSet.sourceFile());
}

std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupConfiguration::findGroup(const FhirCodeSystem& codeSystem) const
{
    return findGroupInternal(codeSystem.getUrl(), codeSystem.getVersion(), codeSystem.sourceFile(),
                             codeSystem.getContentType() == FhirCodeSystem::ContentType::not_present);
}

std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupConfiguration::findGroup(const FhirStructureDefinition& stuctureDefinition) const
{
    return findGroup(stuctureDefinition.url(), stuctureDefinition.version(), stuctureDefinition.sourceFile());
}

std::shared_ptr<const FhirResourceGroup>
FhirResourceGroupConfiguration::findGroupInternal(const std::string& url, const FhirVersion& version,
                                          const std::filesystem::path& sourceFile, bool inferior) const
{
    using namespace std::string_literals;
    std::shared_ptr<FhirResourceGroup> result;
    for (const auto& group : mGroups | std::views::values)
    {
        if (group->addIfMatch(url, version, sourceFile, inferior))
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
