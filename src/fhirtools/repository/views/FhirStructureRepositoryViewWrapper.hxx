// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_FORWARDINGFHIRSTRUCTUREREPOSITORYVIEW_H
#define FHIRTOOLS_FORWARDINGFHIRSTRUCTUREREPOSITORYVIEW_H

#include "FhirStructureRepositoryView.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <string>

namespace fhirtools
{

/**
 * @brief Forward everything except id calls to another view
 *
 * This class is intended to be used as base class for other views
 */
class FhirStructureRepositoryViewWrapper : public FhirStructureRepositoryView
{
public:
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const override;

    FhirStructureRepositoryView::ContentReferenceResolution
    resolveContentReference(const FhirResourceGroup& group, const FhirElement& element) const override;

    [[nodiscard]] const FhirStructureDefinition* systemTypeInteger() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeDecimal() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeDateTime() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeTime() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeDate() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeString() const override;
    [[nodiscard]] const FhirStructureDefinition* systemTypeBoolean() const override;
    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirCodeSystemCodes>
    findCodeSystemCodes(const DefinitionKey& key) const override;

    [[nodiscard]] std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                    const FhirVersion& version) const override;
    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirValueSetCodes> findValueSetCodes(const DefinitionKey& key) const override;
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const override;

    [[nodiscard]] std::optional<DefinitionKey> findRenderVersion(const DefinitionKey& key) const override;

    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;

    std::string_view id() const override;

protected:
    explicit FhirStructureRepositoryViewWrapper(
        std::string viewId, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView);

    const FhirStructureRepositoryView& baseView() const;


private:
    std::string mId;
    std::shared_ptr<const FhirStructureRepositoryView> mBaseView;
};

}

#endif// FHIRTOOLS_FORWARDINGFHIRSTRUCTUREREPOSITORYVIEW_H
