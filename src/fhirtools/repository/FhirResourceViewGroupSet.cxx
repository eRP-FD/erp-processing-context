// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH
#include "FhirResourceViewGroupSet.hxx"
#include "FhirResourceGroup.hxx"
#include "fhirtools/FPExpect.hxx"


namespace fhirtools
{


FhirResourceViewGroupSet::FhirResourceViewGroupSet(std::string initId, std::set<GroupSharedRef> groups,
                                                   gsl::not_null<const FhirStructureRepositoryBackend*> backend)
    : DefaultFhirStructureRepositoryView{std::move(initId), std::move(backend)}
    , mGroups{std::move(groups)}
{
}

FhirResourceViewGroupSet::FhirResourceViewGroupSet(std::string initId, GroupSharedRef group,
                                                   gsl::not_null<const FhirStructureRepositoryBackend*> backend)
    : FhirResourceViewGroupSet{std::move(initId), std::set{gsl::not_null{std::move(group)}}, std::move(backend)}
{
}


const FhirStructureDefinition* fhirtools::FhirResourceViewGroupSet::findStructure(const DefinitionKey& key) const
{

    if (auto fullKey = findVersion(key); fullKey.version.has_value())
    {
        return DefaultFhirStructureRepositoryView::findStructure(fullKey);
    }
    return nullptr;
}


const FhirCodeSystem* FhirResourceViewGroupSet::findCodeSystem(const DefinitionKey& key) const
{
    if (auto fullKey = findVersion(key); fullKey.version.has_value())
    {
        return DefaultFhirStructureRepositoryView::findCodeSystem(fullKey);
    }
    return nullptr;
}

const FhirValueSet* fhirtools::FhirResourceViewGroupSet::findValueSet(const DefinitionKey& key) const
{
    if (auto fullKey = findVersion(key); fullKey.version.has_value())
    {
        return DefaultFhirStructureRepositoryView::findValueSet(fullKey);
    }
    return nullptr;
}

DefinitionKey FhirResourceViewGroupSet::findVersion(DefinitionKey origKey) const
{
    DefinitionKey result{origKey.url, std::nullopt};
    for (const auto& group : mGroups)
    {
        auto fullKey = group->find(origKey).first;
        if (fullKey.version)
        {
            if (origKey.version && *origKey.version != *fullKey.version)
            {
                return {origKey.url, std::nullopt};
            }
            return fullKey;
        }
    }
    return {origKey.url, std::nullopt};
}

}
