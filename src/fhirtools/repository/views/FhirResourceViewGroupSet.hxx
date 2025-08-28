// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX
#define FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX


#include "DefaultFhirStructureRepositoryView.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <memory>
#include <set>

namespace fhirtools
{

struct DefinitionKey;
class FhirResourceGroup;
class FhirStructureRepositoryBackend;

class FhirResourceViewGroupSet : public DefaultFhirStructureRepositoryView
{
public:
    using GroupSharedRef = gsl::not_null<std::shared_ptr<const FhirResourceGroup>>;
    static std::shared_ptr<FhirResourceViewGroupSet> create(std::string initId, std::set<GroupSharedRef> groups,
                             gsl::not_null<const FhirStructureRepositoryBackend*> backend);
    static std::shared_ptr<FhirResourceViewGroupSet> create(std::string initId, GroupSharedRef group,
                             gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;

    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& key) const override;

    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const override;
    [[nodiscard]] std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                    const FhirVersion& version) const override;

    [[nodiscard]] std::optional<DefinitionKey> findRenderVersion(const DefinitionKey& key) const override;


protected:
    struct Construct;
    FhirResourceViewGroupSet(std::string initId, std::set<GroupSharedRef> groups,
                             gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    DefinitionKey findVersion(const DefinitionKey& origKey) const;

    std::set<GroupSharedRef> mGroups;
};

}// namespace fhirtools

#endif// FHIRTOOLS_FHIRRESOURCEVIEWSINGLEGROUP_HXX
