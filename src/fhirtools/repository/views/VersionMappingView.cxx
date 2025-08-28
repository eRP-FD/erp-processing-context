/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "VersionMappingView.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"

#include <rapidjson/document.h>
#include <ranges>
#include <regex>
#include <stdexcept>

using namespace fhirtools;

std::shared_ptr<VersionMappingView>
fhirtools::VersionMappingView::create(std::string id, VersionMapper mapper,
                                      gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
{
    struct Construct : VersionMappingView {
        Construct(std::string id, VersionMapper mapper, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
            : VersionMappingView{std::move(id), std::move(mapper), std::move(baseView)}
        {
        }
    };
    return std::make_shared<Construct>(std::move(id), std::move(mapper), std::move(baseView));
}


VersionMappingView::VersionMappingView(std::string id, VersionMapper mapper,
                                       gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
    : FhirStructureRepositoryViewWrapper{std::move(id), std::move(baseView)}
    , mMapper{std::move(mapper)}
{
}

fhirtools::VersionMappingView::~VersionMappingView() = default;

const FhirCodeSystem* VersionMappingView::findCodeSystem(const DefinitionKey& key) const
{
    if (key.version)
    {
        return FhirStructureRepositoryViewWrapper::findCodeSystem(realKey(key.url, *key.version));
    }
    return FhirStructureRepositoryViewWrapper::findCodeSystem(key);
}

std::shared_ptr<const FhirCodeSystemCodes> VersionMappingView::findCodeSystemCodes(const DefinitionKey& key) const
{
    const auto* codeSystem = findCodeSystem(key);
    return codeSystem ? std::make_shared<FhirCodeSystemCodes>(codeSystem->getCodes(*this)) : nullptr;
}

std::set<const FhirCodeSystem*> VersionMappingView::findSupplementers(const std::string& url,
                                                                       const FhirVersion& version) const
{
    auto result = FhirStructureRepositoryViewWrapper::findSupplementers(url, version);
    if (auto renderVersion = mMapper.renderVersion(url, version); renderVersion != version)
    {
        result.merge(FhirStructureRepositoryViewWrapper::findSupplementers(url, renderVersion));
    }
    return result;
}

const FhirValueSet* VersionMappingView::findValueSet(const DefinitionKey& key) const
{
    if (key.version)
    {
        return FhirStructureRepositoryViewWrapper::findValueSet(realKey(key.url, *key.version));
    }
    return FhirStructureRepositoryViewWrapper::findValueSet(key);
}

std::shared_ptr<const FhirValueSetCodes> VersionMappingView::findValueSetCodes(const DefinitionKey& key) const
{
    const auto* valueSet = findValueSet(key);
    return valueSet ? FhirValueSetCodes::create(this, valueSet) : nullptr;
}

const FhirStructureDefinition* VersionMappingView::findStructure(const DefinitionKey& key) const
{
    if (key.version)
    {
        return FhirStructureRepositoryViewWrapper::findStructure(realKey(key.url, *key.version));
    }
    return FhirStructureRepositoryViewWrapper::findStructure(key);
}

std::optional<DefinitionKey> VersionMappingView::findRenderVersion(const DefinitionKey& key) const
{
    const auto* profile = findStructure(key);
    if (!profile)
    {
        return std::nullopt;
    }
    return std::optional<DefinitionKey>{std::in_place, key.url, mMapper.renderVersion(key.url, profile->version())};
}
DefinitionKey fhirtools::VersionMappingView::realKey(std::string url, const FhirVersion& version) const
{
    auto realVersion = mMapper.realVersion(url, version);
    return {std::move(url), std::move(realVersion)};
}
