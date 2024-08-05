// (C) Copyright IBM Deutschland GmbH 2024
// (C) Copyright IBM Corp. 2024

#ifndef FHIRTOOLS_FHIRGROUPCONFIGURATION
#define FHIRTOOLS_FHIRGROUPCONFIGURATION
#include "FhirResourceGroup.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <rapidjson/fwd.h>
#include <map>
#include <memory>

namespace fhirtools
{

class FhirResourceGroupConfiguration : public FhirResourceGroupResolver
{
public:
    FhirResourceGroupConfiguration(const gsl::not_null<const rapidjson::Value*>& fhirResourceGroups);

    std::map<std::string, std::shared_ptr<const FhirResourceGroup>> allGroups() const override;

    std::shared_ptr<const FhirResourceGroup> findGroup(const std::string& url, const FhirVersion& version,
                                                       const std::filesystem::path& sourceFile) const override;
    std::shared_ptr<const FhirResourceGroup> findGroupById(const std::string& id) const override;

    ~FhirResourceGroupConfiguration() override;

    FhirResourceGroupConfiguration(const FhirResourceGroupConfiguration&) = delete;
    FhirResourceGroupConfiguration(FhirResourceGroupConfiguration&&) = default;
    FhirResourceGroupConfiguration& operator=(const FhirResourceGroupConfiguration&) = delete;
    FhirResourceGroupConfiguration& operator=(FhirResourceGroupConfiguration&&) = default;

private:
    class Group;
    std::map<std::string, std::shared_ptr<Group>> mGroups;
};

}// namespace fhirtools

#endif// FHIRTOOLS_FHIRGROUPCONFIGURATION
