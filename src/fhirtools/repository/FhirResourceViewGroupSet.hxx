#ifndef FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX
#define FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX


#include "FhirStructureRepository.hxx"
#include "fhirtools/util/Gsl.hxx"

namespace fhirtools
{

class FhirResourceGroup;

class FhirResourceViewGroupSet : public DefaultFhirStructureRepositoryView
{
public:
    using GroupSharedRef = gsl::not_null<std::shared_ptr<const FhirResourceGroup>>;
    FhirResourceViewGroupSet(std::string initId, std::set<GroupSharedRef> groups,
                             gsl::not_null<const FhirStructureRepositoryBackend*> backend);
    FhirResourceViewGroupSet(std::string initId, GroupSharedRef group,
                             gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;

    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& key) const override;

    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const override;


private:
    DefinitionKey findVersion(DefinitionKey origKey) const;

    std::set<GroupSharedRef> mGroups;
};

}// namespace fhirtools

#endif// FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX
