// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_FHIRGROUPRESOLVER_HXX
#define FHIRTOOLS_FHIRGROUPRESOLVER_HXX

#include "FhirVersion.hxx"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace fhirtools
{
struct DefinitionKey;

class FhirResourceGroup
{
public:
    virtual std::string_view id() const = 0;
    virtual std::optional<FhirVersion> findVersionLocal(const std::string& url) const = 0;
    virtual std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
    findVersion(const std::string& url) const = 0;

    std::pair<DefinitionKey, std::shared_ptr<const FhirResourceGroup>> find(const DefinitionKey& key) const;

    virtual ~FhirResourceGroup() = default;
};

class FhirResourceGroupResolver
{
public:
    virtual std::map<std::string, std::shared_ptr<const FhirResourceGroup>> allGroups() const = 0;
    virtual std::shared_ptr<const FhirResourceGroup> findGroup(const std::string& url, const FhirVersion& version,
                                                               const std::filesystem::path& sourceFile) const = 0;
    virtual std::shared_ptr<const FhirResourceGroup> findGroupById(const std::string& id) const = 0;
    virtual ~FhirResourceGroupResolver() = default;
};


}// namespace fhirtools

#endif// FHIRTOOLS_FHIRGROUPRESOLVER_HXX
