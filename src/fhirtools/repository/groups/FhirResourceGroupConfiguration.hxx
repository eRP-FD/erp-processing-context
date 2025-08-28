// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_FHIRGROUPCONFIGURATION
#define FHIRTOOLS_FHIRGROUPCONFIGURATION
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <rapidjson/fwd.h>
#include <map>
#include <memory>

namespace fhirtools
{
class VersionMapper;

class FhirResourceGroupConfiguration : public FhirResourceGroupResolver
{
public:
    FhirResourceGroupConfiguration(const gsl::not_null<const rapidjson::Value*>& fhirResourceGroups,
                                   const std::shared_ptr<const VersionMapper>& versionMapper);

    std::map<std::string, std::shared_ptr<const FhirResourceGroup>> allGroups() const override;


    std::shared_ptr<const FhirResourceGroup> findGroupById(const std::string& id) const override;

    std::shared_ptr<const FhirResourceGroup> findGroup(const FhirStructureDefinition& stuctureDefinition) const override;
    std::shared_ptr<const FhirResourceGroup> findGroup(const FhirCodeSystem& codeSystem) const override;
    std::shared_ptr<const FhirResourceGroup> findGroup(const FhirValueSet& valueSet) const override;

    ~FhirResourceGroupConfiguration() override;

    FhirResourceGroupConfiguration(const FhirResourceGroupConfiguration&) = delete;
    FhirResourceGroupConfiguration(FhirResourceGroupConfiguration&&) = default;
    FhirResourceGroupConfiguration& operator=(const FhirResourceGroupConfiguration&) = delete;
    FhirResourceGroupConfiguration& operator=(FhirResourceGroupConfiguration&&) = default;

private:
    std::shared_ptr<const FhirResourceGroup> findGroup(const std::string& url, const FhirVersion& version,
                                                       const std::filesystem::path& sourceFile) const override;

    std::shared_ptr<const FhirResourceGroup> findGroupInternal(const std::string& url, const FhirVersion& version,
                                                               const std::filesystem::path& sourceFile,
                                                               bool inferior) const;
    class Group;
    std::map<std::string, std::shared_ptr<Group>> mGroups;
};

}// namespace fhirtools

#endif// FHIRTOOLS_FHIRGROUPCONFIGURATION
