// (C) Copyright IBM Deutschland GmbH 2024
// (C) Copyright IBM Corp. 2024

#include "DefinitionKey.hxx"
#include "FhirResourceGroup.hxx"
#include "fhirtools/FPExpect.hxx"

namespace fhirtools
{

std::pair<DefinitionKey, std::shared_ptr<const FhirResourceGroup>>
FhirResourceGroup::find(const DefinitionKey& key) const
{
    std::pair<DefinitionKey, std::shared_ptr<const FhirResourceGroup>> result{key, nullptr};
    std::tie(result.first.version, result.second) = findVersion(result.first.url);
    if (! result.first.version.has_value())
    {
        TVLOG(2) << "resource is not member of group " << id() << ": " << key;
        return result;
    }
    if (! key.version || key.version == result.first.version)
    {
        TVLOG(3) << "version found for " << result.first.url << " in group " << id() << ": " << *result.first.version;
        return result;
    }
    TVLOG(2) << "version mismatch for " << result.first.url << " in group " << id() << ": " << *key.version
             << "(requested) vs. " << *result.first.version << "(found)";
    return std::make_pair(DefinitionKey{result.first.url, std::nullopt}, nullptr);
}

}