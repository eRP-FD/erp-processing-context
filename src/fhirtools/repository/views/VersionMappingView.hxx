/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#ifndef FHIRTOOLS_VERSIONMAPPINGVIEW_H
#define FHIRTOOLS_VERSIONMAPPINGVIEW_H

#include "FhirStructureRepositoryViewWrapper.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "fhirtools/repository/VersionMapper.hxx"

#include <rapidjson/fwd.h>
#include <tuple>

namespace fhirtools
{

class VersionMappingView: public FhirStructureRepositoryViewWrapper
{
public:

    [[nodiscard]] static std::shared_ptr<VersionMappingView>
    create(std::string id, VersionMapper mapper,
           gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView);

    ~VersionMappingView() override;

    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirCodeSystemCodes>
    findCodeSystemCodes(const DefinitionKey& key) const override;
    [[nodiscard]] std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                     const FhirVersion& version) const override;
    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirValueSetCodes> findValueSetCodes(const DefinitionKey& key) const override;
    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;
    [[nodiscard]] std::optional<DefinitionKey> findRenderVersion(const DefinitionKey& key) const override;

private:
    explicit VersionMappingView(std::string id, VersionMapper mapper,
                                gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView);

    DefinitionKey realKey(std::string url, const FhirVersion& version) const;

    VersionMapper mMapper;

};


}

#endif// FHIRTOOLS_VERSIONMAPPINGVIEW_H
