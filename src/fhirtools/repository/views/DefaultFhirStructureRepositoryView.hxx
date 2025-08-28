/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIRTOOLS_DEFAULTFHIRSTRUCTUREREPOSITORYVIEW_H
#define FHIRTOOLS_DEFAULTFHIRSTRUCTUREREPOSITORYVIEW_H

#include "FhirStructureRepositoryView.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <memory>

namespace fhirtools
{

class FhirElement;
class FhirResourceGroup;
class FhirStructureRepositoryBackend;

class DefaultFhirStructureRepositoryView : public FhirStructureRepositoryView
{
public:
    static std::shared_ptr<DefaultFhirStructureRepositoryView>
    create(std::string initId, gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    [[nodiscard]] std::string_view id() const override;
    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const override;
    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirValueSetCodes> findValueSetCodes(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirCodeSystemCodes>
    findCodeSystemCodes(const DefinitionKey& key) const override;
    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const override;
    [[nodiscard]] std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                    const FhirVersion& version) const override;
    [[nodiscard]] std::optional<DefinitionKey> findRenderVersion(const DefinitionKey& key) const override;

    const FhirStructureDefinition* systemTypeBoolean() const override;
    const FhirStructureDefinition* systemTypeString() const override;
    const FhirStructureDefinition* systemTypeDate() const override;
    const FhirStructureDefinition* systemTypeTime() const override;
    const FhirStructureDefinition* systemTypeDateTime() const override;
    const FhirStructureDefinition* systemTypeDecimal() const override;
    const FhirStructureDefinition* systemTypeInteger() const override;

    [[nodiscard]] ContentReferenceResolution resolveContentReference(const FhirResourceGroup& group,
                                                                     const FhirElement& element) const override;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const override;

protected:
    struct Construct;
    explicit DefaultFhirStructureRepositoryView(std::string initId,
                                                gsl::not_null<const FhirStructureRepositoryBackend*> backend);
private:
    std::string mId;
    gsl::not_null<const FhirStructureRepositoryBackend*> mBackend;
};
}

#endif// FHIRTOOLS_DEFAULTFHIRSTRUCTUREREPOSITORYVIEW_H
