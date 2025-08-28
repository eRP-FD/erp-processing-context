// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirResourceGroup.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"

namespace fhirtools
{

std::pair<DefinitionKey, std::shared_ptr<const FhirResourceGroup>>
FhirResourceGroup::find(const DefinitionKey& key) const
{
    std::pair<DefinitionKey, std::shared_ptr<const FhirResourceGroup>> result{key, nullptr};
    std::tie(result.first.version, result.second) = findVersion(result.first);
    return result;
}

std::shared_ptr<const FhirResourceGroup> FhirResourceGroupResolver::findGroup(const FhirCodeSystem& codeSystem) const
{
    return findGroup(codeSystem.getUrl(), codeSystem.getVersion(), codeSystem.sourceFile());
}

std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupResolver::findGroup(const FhirValueSet& valueSet) const
{
    return findGroup(valueSet.getUrl(), valueSet.getVersion(), valueSet.sourceFile());
}

std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupResolver::findGroup(const FhirStructureDefinition& stuctureDefinition) const
{
    return findGroup(stuctureDefinition.url(), stuctureDefinition.version(), stuctureDefinition.sourceFile());
}

}
