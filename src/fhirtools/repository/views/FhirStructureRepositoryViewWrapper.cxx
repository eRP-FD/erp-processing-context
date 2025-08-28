// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirStructureRepositoryViewWrapper.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"

#include <utility>

namespace fhirtools
{

fhirtools::FhirStructureRepositoryViewWrapper::FhirStructureRepositoryViewWrapper(
    std::string viewId, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
    : mId(std::move(viewId))
    , mBaseView{std::move(baseView)}
{
}


std::tuple<ProfiledElementTypeInfo, size_t>
FhirStructureRepositoryViewWrapper::resolveBaseContentReference(std::string_view elementId) const
{
    return mBaseView->resolveBaseContentReference(elementId);
}

FhirStructureRepositoryView::ContentReferenceResolution
FhirStructureRepositoryViewWrapper::resolveContentReference(const FhirResourceGroup& group,
                                                            const FhirElement& element) const
{
    return mBaseView->resolveContentReference(group, element);
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeInteger() const
{
    return mBaseView->systemTypeInteger();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeDecimal() const
{
    return mBaseView->systemTypeDecimal();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeDateTime() const
{
    return mBaseView->systemTypeDateTime();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeTime() const
{
    return mBaseView->systemTypeTime();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeDate() const
{
    return mBaseView->systemTypeDate();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeString() const
{
    return mBaseView->systemTypeString();
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::systemTypeBoolean() const
{
    return mBaseView->systemTypeBoolean();
}

const FhirCodeSystem* FhirStructureRepositoryViewWrapper::findCodeSystem(const DefinitionKey& key) const
{
    return mBaseView->findCodeSystem(key);
}

std::shared_ptr<const FhirCodeSystemCodes>
FhirStructureRepositoryViewWrapper::findCodeSystemCodes(const DefinitionKey& key) const
{
    return mBaseView->findCodeSystemCodes(key);
}

std::set<const FhirCodeSystem*> FhirStructureRepositoryViewWrapper::findSupplementers(const std::string& url,
                                                                                      const FhirVersion& version) const
{
    return mBaseView->findSupplementers(url, version);
}

const FhirValueSet* FhirStructureRepositoryViewWrapper::findValueSet(const DefinitionKey& key) const
{
    return mBaseView->findValueSet(key);
}

std::shared_ptr<const FhirValueSetCodes>
FhirStructureRepositoryViewWrapper::findValueSetCodes(const DefinitionKey& key) const
{
    return mBaseView->findValueSetCodes(key);
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::findTypeById(const std::string& typeName) const
{
    return mBaseView->findTypeById(typeName);
}

std::optional<DefinitionKey>
fhirtools::FhirStructureRepositoryViewWrapper::findRenderVersion(const DefinitionKey& key) const
{
    return mBaseView->findRenderVersion(key);
}

const FhirStructureDefinition* FhirStructureRepositoryViewWrapper::findStructure(const DefinitionKey& key) const
{
    return mBaseView->findStructure(key);
}

std::string_view FhirStructureRepositoryViewWrapper::id() const
{
    return mId;
}
const FhirStructureRepositoryView& fhirtools::FhirStructureRepositoryViewWrapper::baseView() const
{
    return *mBaseView;
}
}// namespace fhirtools
