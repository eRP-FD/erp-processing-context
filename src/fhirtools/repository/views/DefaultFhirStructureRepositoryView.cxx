/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "DefaultFhirStructureRepositoryView.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"

namespace fhirtools
{
struct DefaultFhirStructureRepositoryView::Construct: DefaultFhirStructureRepositoryView {
    template<typename... Ts>
    Construct(Ts&&... args) : DefaultFhirStructureRepositoryView{std::forward<Ts>(args)...}{}
};

std::shared_ptr<DefaultFhirStructureRepositoryView>
DefaultFhirStructureRepositoryView::create(std::string initId,
                                           gsl::not_null<const FhirStructureRepositoryBackend*> backend)
{
    return std::make_shared<Construct>(std::move(initId), std::move(backend));
}

DefaultFhirStructureRepositoryView::DefaultFhirStructureRepositoryView(
    std::string initId, gsl::not_null<const FhirStructureRepositoryBackend*> backend)
    : mId{std::move(initId)}
    , mBackend(std::move(backend))
{
}

std::string_view fhirtools::DefaultFhirStructureRepositoryView::id() const
{
    return mId;
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::findStructure(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown StructureDefinition: " + key.url);
    return mBackend->findDefinition(key.url, *key.version);
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::findTypeById(const std::string& typeName) const
{
    return mBackend->findTypeById(typeName);
}

const FhirValueSet* DefaultFhirStructureRepositoryView::findValueSet(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown ValueSet: " + key.url);
    return mBackend->findValueSet(key.url, *key.version);
}

std::shared_ptr<const FhirValueSetCodes>
DefaultFhirStructureRepositoryView::findValueSetCodes(const DefinitionKey& key) const
{
    const auto* valueSet = findValueSet(key);
    return valueSet ? FhirValueSetCodes::create(this, valueSet) : nullptr;
}

const FhirCodeSystem* DefaultFhirStructureRepositoryView::findCodeSystem(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown CodeSystem: " + key.url);
    return mBackend->findCodeSystem(key.url, *key.version);
}

std::shared_ptr<const FhirCodeSystemCodes>
DefaultFhirStructureRepositoryView::findCodeSystemCodes(const DefinitionKey& key) const
{
    const auto* codeSystem = findCodeSystem(key);
    return codeSystem ? std::make_shared<FhirCodeSystemCodes>(codeSystem->getCodes(*this)) : nullptr;
}

std::set<const FhirCodeSystem*> DefaultFhirStructureRepositoryView::findSupplementers(const std::string& url,
                                                                                      const FhirVersion& version) const
{
    return mBackend->findSupplementers(url, version);
}

std::optional<DefinitionKey> DefaultFhirStructureRepositoryView::findRenderVersion(const DefinitionKey& key) const
{
    if (const auto* structure = findStructure(key))
    {
        return structure->key();
    }
    return std::nullopt;
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeBoolean() const
{
    return mBackend->systemTypeBoolean();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeString() const
{
    return mBackend->systemTypeString();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDate() const
{
    return mBackend->systemTypeDate();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeTime() const
{
    return mBackend->systemTypeTime();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDateTime() const
{
    return mBackend->systemTypeDateTime();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDecimal() const
{
    return mBackend->systemTypeDecimal();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeInteger() const
{
    return mBackend->systemTypeInteger();
}

}// namespace fhirtools
