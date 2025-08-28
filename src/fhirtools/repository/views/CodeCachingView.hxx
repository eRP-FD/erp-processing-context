/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryViewWrapper.hxx"

#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace fhirtools
{
class FhirCodeSystemCodes;
class FhirValueSetCodes;

class CodeCachingView : public FhirStructureRepositoryViewWrapper
{
public:
    static std::shared_ptr<CodeCachingView>
    create(std::string viewId, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView);

    [[nodiscard]] std::shared_ptr<const FhirCodeSystemCodes>
    findCodeSystemCodes(const DefinitionKey& key) const override;
    [[nodiscard]] std::shared_ptr<const FhirValueSetCodes> findValueSetCodes(const DefinitionKey& key) const override;

private:
    explicit CodeCachingView(std::string viewId,
                             gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView);


    void baseFindCodes(std::shared_ptr<const FhirCodeSystemCodes>& out, const DefinitionKey& key) const;
    void baseFindCodes(std::shared_ptr<const FhirValueSetCodes>& out, const DefinitionKey& key) const;

    template<typename ValueT>
    ValueT findCodes(std::unordered_map<DefinitionKey, ValueT>& map, const DefinitionKey& key,
                     std::shared_mutex& mutex) const;


    mutable std::shared_mutex mCodeSystemCodesMutex;
    mutable std::unordered_map<DefinitionKey, std::shared_ptr<const FhirCodeSystemCodes>> mCodeSystemCodes;
    mutable std::shared_mutex mValueSetCodesMutex;
    mutable std::unordered_map<DefinitionKey, std::shared_ptr<const FhirValueSetCodes>> mValueSetCodes;
};

}
