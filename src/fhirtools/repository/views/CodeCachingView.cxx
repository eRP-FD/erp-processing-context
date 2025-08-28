/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/views/CodeCachingView.hxx"


namespace fhirtools
{
std::shared_ptr<CodeCachingView>
CodeCachingView::create(std::string viewId, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
{
    struct Construct : CodeCachingView {
        Construct(std::string viewId, gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
            : CodeCachingView{std::move(viewId), std::move(baseView)}
        {
        }
    };
    return std::make_shared<Construct>(std::move(viewId), std::move(baseView));
}

CodeCachingView::CodeCachingView(std::string viewId,
                                 gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>> baseView)
    : FhirStructureRepositoryViewWrapper{std::move(viewId), std::move(baseView)}
{
}

std::shared_ptr<const FhirCodeSystemCodes> CodeCachingView::findCodeSystemCodes(const DefinitionKey& key) const
{
    return findCodes(mCodeSystemCodes, key, mCodeSystemCodesMutex);
}

std::shared_ptr<const FhirValueSetCodes> CodeCachingView::findValueSetCodes(const DefinitionKey& key) const
{
    return findCodes(mValueSetCodes, key, mValueSetCodesMutex);
}

void CodeCachingView::baseFindCodes(std::shared_ptr<const FhirCodeSystemCodes>& out, const DefinitionKey& key) const
{
    out = baseView().findCodeSystemCodes(key);
}

void CodeCachingView::baseFindCodes(std::shared_ptr<const FhirValueSetCodes>& out, const DefinitionKey& key) const
{
    out = baseView().findValueSetCodes(key);
}


template<typename ValueT>
ValueT CodeCachingView::findCodes(std::unordered_map<DefinitionKey, ValueT>& map, const DefinitionKey& key,
                                  std::shared_mutex& mutex) const
{
    std::shared_lock read{mutex};
    if (const auto cached = map.find(key); cached != map.end())
    {
        return cached->second;
    }
    read.unlock();
    ValueT codes;
    baseFindCodes(codes, key);
    if (! codes)
    {
        return nullptr;
    }
    std::lock_guard write{mutex};
    auto [it, inserted] = map.try_emplace(codes->key(), codes);
    if (! inserted)
    {
        codes = it->second;
    }
    if (! key.version)
    {
        map.insert_or_assign(key, codes);
    }
    return codes;
}


}
