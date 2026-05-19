/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/VersionMapper.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"

#include <rapidjson/document.h>
#include <ranges>
#include <regex>

namespace fhirtools
{

VersionMapper::Config::Mapping VersionMapper::Config::Mapping::fromJson(const rapidjson::Value& mapping)
{
    using namespace std::string_literals;
    Expect3(mapping.IsObject(), "baseElement must be an array of objects", std::logic_error);
    std::string urlRegex;
    std::string versionRegex;
    std::string realVersion;
    std::string renderVersion;
    for (const auto& mappingField : mapping.GetObject())
    {
        Expect3(mappingField.value.IsString(),
                "field "s.append(mappingField.name.GetString()).append(" must be string."), std::logic_error);
        if (mappingField.name == "urlRegex")
        {
            urlRegex.assign(mappingField.value.GetString(), mappingField.value.GetStringLength());
        }
        else if (mappingField.name == "versionRegex")
        {
            versionRegex.assign(mappingField.value.GetString(), mappingField.value.GetStringLength());
        }
        else if (mappingField.name == "realVersion")
        {
            realVersion.assign(mappingField.value.GetString(), mappingField.value.GetStringLength());
        }
        else if (mappingField.name == "renderVersion")
        {
            renderVersion.assign(mappingField.value.GetString(), mappingField.value.GetStringLength());
        }
        else
        {
            Fail2("unknown field in mapping definition: "s.append(mappingField.name.GetString()), std::logic_error);
        }
    }
    Expect3(! urlRegex.empty(), "urlRegex missing or empty.", std::logic_error);
    Expect3(! versionRegex.empty(), "versionRegex missing or empty.", std::logic_error);
    Expect3(! realVersion.empty(), "realVersion missing or empty.", std::logic_error);
    Expect3(! renderVersion.empty(), "renderVersion missing or empty.", std::logic_error);
    return {
        .urlRegex = std::move(urlRegex),
        .versionRegex = std::move(versionRegex),
        .realVersion = FhirVersion{std::move(realVersion)},
        .renderVersion = FhirVersion{std::move(renderVersion)},
    };
}

VersionMapper::Config VersionMapper::Config::fromJson(const rapidjson::Value& baseElement)
{
    VersionMapper::Config result;
    using namespace std::string_literals;
    Expect3(baseElement.IsArray(), "baseElement must be an array", std::logic_error);
    for (const auto& mapping : baseElement.GetArray())
    {
        result.map.emplace_back(Mapping::fromJson(mapping));
    }
    return result;
}

struct VersionMapper::Mapping {
    Mapping(VersionMapper::Config::Mapping map)
        : urlRegex{map.urlRegex, std::regex_constants::ECMAScript | std::regex_constants::optimize}
        , versionRegex{map.versionRegex, std::regex_constants::ECMAScript | std::regex_constants::optimize}
        , mapping{std::move(map)}
    {
    }
    std::regex urlRegex;
    std::regex versionRegex;
    Config::Mapping mapping;
};


VersionMapper::VersionMapper(Config config)
    : mMaps{toMapping(std::move(config.map))}
{
}
VersionMapper::~VersionMapper() = default;
VersionMapper::VersionMapper(const VersionMapper&) = default;
VersionMapper::VersionMapper(VersionMapper&&) noexcept = default;
VersionMapper& VersionMapper::operator=(const VersionMapper&) = default;
VersionMapper& VersionMapper::operator=(VersionMapper&&) noexcept = default;

std::list<VersionMapper::Mapping> VersionMapper::toMapping(std::list<Config::Mapping> mapping)
{
    auto transformed = std::ranges::transform_view(std::move(mapping), [](auto map) -> Mapping {
        return {std::move(map)};
    });
    std::list<VersionMapper::Mapping> result;
    std::ranges::move(transformed, std::back_inserter(result));
    return result;
}

const VersionMapper::Config::Mapping* VersionMapper::findMapping(const DefinitionKey& foundKey,
                                                                 const DefinitionKey& queryKey) const
{
    Expect3(queryKey.url == foundKey.url,
            "baseView returned a different url than queried: " + to_string(queryKey) + " != " + to_string(foundKey),
            std::logic_error);
    auto matchingMapping = std::ranges::find_if(mMaps, [&](const Mapping& mapping) -> bool {
        return foundKey.version == mapping.mapping.realVersion && std::regex_match(queryKey.url, mapping.urlRegex) &&
               ((not queryKey.version.has_value()) ||
                std::regex_match(to_string(*queryKey.version), mapping.versionRegex));
    });
    return (matchingMapping != mMaps.cend()) ? &matchingMapping->mapping : nullptr;
}

FhirVersion VersionMapper::realVersion(std::string_view url, FhirVersion version) const
{
    const auto& versionStr = to_string(version);
    auto map = std::ranges::find_if(mMaps, [&](const Mapping& mapping) -> bool {
        return std::regex_match(url.begin(), url.end(), mapping.urlRegex) &&
               std::regex_match(versionStr, mapping.versionRegex);
    });
    if (map == mMaps.end())
    {
        return version;
    }
    return map->mapping.realVersion;
}

FhirVersion VersionMapper::renderVersion(std::string_view url, const FhirVersion& version) const
{
    auto map = std::ranges::find_if(mMaps, [&](const Mapping& mapping) -> bool {
        return std::regex_match(url.begin(), url.end(), mapping.urlRegex) && version == mapping.mapping.realVersion;
    });
    if (map == mMaps.end())
    {
        return version;
    }
    return map->mapping.renderVersion;
}

}// namespace fhirtools
