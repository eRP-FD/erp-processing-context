/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "fhirtools/repository/FhirVersion.hxx"

#include <rapidjson/fwd.h>

namespace fhirtools
{
struct DefinitionKey;

class VersionMapper
{
public:
    struct Config {
        static Config fromJson(const rapidjson::Value& baseElement);
        struct Mapping {
            static Mapping fromJson(const rapidjson::Value& mapping);
            std::string urlRegex;
            std::string versionRegex;
            FhirVersion realVersion;
            FhirVersion renderVersion;
            bool operator==(const Mapping&) const = default;
        };
        std::list<Mapping> map;
        bool operator==(const Config&) const = default;
    };
    explicit VersionMapper(Config config);
    ~VersionMapper();

    FhirVersion realVersion(std::string_view url, FhirVersion version) const;
    FhirVersion renderVersion(const std::string& url, const FhirVersion& version) const;
    const Config::Mapping* findMapping(const DefinitionKey& foundKey, const DefinitionKey& queryKey) const;

    VersionMapper(const VersionMapper&);
    VersionMapper(VersionMapper&&) noexcept;
    VersionMapper& operator = (const VersionMapper&);
    VersionMapper& operator = (VersionMapper&&) noexcept;
private:
    struct Mapping;

    static std::list<Mapping> toMapping(std::list<Config::Mapping> mapping);

    std::list<Mapping> mMaps;
};


}// namespace fhirtools
