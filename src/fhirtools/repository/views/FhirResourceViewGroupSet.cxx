// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirResourceViewGroupSet.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"


namespace fhirtools
{
struct FhirResourceViewGroupSet::Construct : FhirResourceViewGroupSet {
    template<typename... Ts>
    Construct(Ts&&... args): FhirResourceViewGroupSet{std::forward<Ts>(args)...}{}
};

std::shared_ptr<FhirResourceViewGroupSet>
fhirtools::FhirResourceViewGroupSet::create(std::string initId, std::set<GroupSharedRef> groups,
                                            gsl::not_null<const FhirStructureRepositoryBackend*> backend)
{
    return std::make_shared<Construct>(std::move(initId), std::move(groups), std::move(backend));
}

std::shared_ptr<FhirResourceViewGroupSet>
fhirtools::FhirResourceViewGroupSet::create(std::string initId, GroupSharedRef group,
                                            gsl::not_null<const FhirStructureRepositoryBackend*> backend)
{
    return create(std::move(initId), std::set{std::move(group)}, std::move(backend));
}

FhirResourceViewGroupSet::FhirResourceViewGroupSet(std::string initId, std::set<GroupSharedRef> groups,
                                                   gsl::not_null<const FhirStructureRepositoryBackend*> backend)
    : DefaultFhirStructureRepositoryView{std::move(initId), std::move(backend)}
    , mGroups{std::move(groups)}
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

std::set<const FhirCodeSystem*> FhirResourceViewGroupSet::findSupplementers(const std::string& url,
                                                                             const FhirVersion& version) const
{
    std::set supplementers = DefaultFhirStructureRepositoryView::findSupplementers(url, version);
    std::erase_if(supplementers, [this](const FhirCodeSystem* cs)->bool {
        return ! findVersion(cs->key()).version.has_value();
    });
    return supplementers;
}

std::optional<DefinitionKey> FhirResourceViewGroupSet::findRenderVersion(const DefinitionKey& key) const
{
    if (const auto fullKey = findVersion(key); fullKey.version.has_value())
    {
        return DefaultFhirStructureRepositoryView::findRenderVersion(fullKey);
    }
    return std::nullopt;
}

const FhirValueSet* fhirtools::FhirResourceViewGroupSet::findValueSet(const DefinitionKey& key) const
{
    if (auto fullKey = findVersion(key); fullKey.version.has_value())
    {
        return DefaultFhirStructureRepositoryView::findValueSet(fullKey);
    }
    return nullptr;
}

DefinitionKey FhirResourceViewGroupSet::findVersion(const DefinitionKey& origKey) const
{
    DefinitionKey result{origKey.url, std::nullopt};
    for (const auto& group : mGroups)
    {
        auto fullKey = group->find(origKey).first;
        if (fullKey.version)
        {
            return fullKey;
        }
    }
    return {origKey.url, std::nullopt};
}

}
