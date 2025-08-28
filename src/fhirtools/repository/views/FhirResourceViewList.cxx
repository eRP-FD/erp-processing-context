/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirResourceViewList.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "shared/util/Expect.hxx"

#include <date/date.h>

using namespace fhirtools;

namespace
{
auto containsStructure(DefinitionKey key)
{
    return [key = std::move(key)](const std::shared_ptr<const FhirStructureRepositoryView>& view) {
        return view->findStructure(key) != nullptr;
    };
}
}

FhirResourceViewList::FhirResourceViewList(const FhirStructureRepositoryBackend& backend,
                                           const FhirResourceViewConfiguration::ViewList& viewListConfig)
    : FhirResourceViewList{[&backend](const FhirResourceViewConfiguration::ViewConfig& viewConfig) -> ViewPtr {
                               return viewConfig.view(&backend);
                           },
                           viewListConfig}
{
}

FhirResourceViewList::FhirResourceViewList() = default;
fhirtools::FhirResourceViewList::FhirResourceViewList(const ViewSource& viewSource,
                                                      const FhirResourceViewConfiguration::ViewList& viewListConfig)
{
    struct Entry {
        Entry(const FhirResourceViewConfiguration::ViewConfig& viewConfig, const ViewPtr& initView)
            : start{viewConfig.mStart}
            , end{viewConfig.mEnd}
            , view{initView}
        {
        }
        bool latest(const Entry& rhs) const
        {
            return std::tie(start, end) < std::tie(rhs.start, rhs.end);
        }
        operator std::shared_ptr<const FhirStructureRepositoryView>() &&
        {
            return std::move(view);
        }

        std::optional<date::local_days> start;
        std::optional<date::local_days> end;
        std::shared_ptr<const FhirStructureRepositoryView> view;
    };
    std::vector<Entry> entries;
    entries.reserve(viewListConfig.size());
    for (const auto& viewConfig : viewListConfig)
    {
        entries.emplace_back(*viewConfig, viewSource(*viewConfig));
    }
    std::ranges::sort(entries, &Entry::latest);
    std::ranges::move(entries, std::back_inserter(mSortedViews));
}

bool FhirResourceViewList::empty() const
{
    return mSortedViews.empty();
}

size_t FhirResourceViewList::size() const
{
    return mSortedViews.size();
}

const std::list<std::shared_ptr<const FhirStructureRepositoryView>>& fhirtools::FhirResourceViewList::all() const
{
    return mSortedViews;
}

std::shared_ptr<const FhirStructureRepositoryView> FhirResourceViewList::latest() const
{
    return mSortedViews.back();
}

std::shared_ptr<const FhirStructureRepositoryView> FhirResourceViewList::match(std::string url,
                                                                               FhirVersion version) const
{
    return match(DefinitionKey{std::move(url), std::move(version)});
}

std::shared_ptr<const FhirStructureRepositoryView> FhirResourceViewList::match(DefinitionKey definitionKey) const
{
    using std::ranges::find_if;
    auto result = find_if(mSortedViews.rbegin(), mSortedViews.rend(), containsStructure(std::move(definitionKey)));
    return result != mSortedViews.rend() ? *result : nullptr;
}

FhirResourceViewList fhirtools::FhirResourceViewList::matchAll(const std::string& url, FhirVersion version) const
{
    FhirResourceViewList result;
    std::ranges::copy_if(mSortedViews, std::back_inserter(result.mSortedViews), containsStructure({url, version}));
    return result;
}

std::set<DefinitionKey>
fhirtools::FhirResourceViewList::supportedVersions(const std::list<std::string>& profileUrls) const
{
    std::set<DefinitionKey> result;
    for (const auto& view : mSortedViews)
    {
        for (const auto& url : profileUrls)
        {
            const DefinitionKey key{url, std::nullopt};
            if (const auto* profile = view->findStructure(key))
            {
                result.emplace(profile->key());
            }
        }
    }
    return result;
}

DefinitionKey FhirResourceViewList::latestRenderVersion(const std::string& profileUrl) const
{
    std::set<DefinitionKey> supported;
    for (const auto& view : mSortedViews)
    {
        const DefinitionKey key{profileUrl, std::nullopt};
        if (auto renderVersion = view->findRenderVersion(key))
        {
            supported.emplace(*renderVersion);
        }
    }
    FPExpect(! supported.empty(), "latestRenderVersion: profileType not supported: " + profileUrl);
    return std::ranges::max(supported, {}, &DefinitionKey::version);
}

FhirResourceViewList fhirtools::FhirResourceViewList::transform(
    const std::function<std::shared_ptr<const FhirStructureRepositoryView>(
        gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>>)>& func) &&
{
    FhirResourceViewList result;
    for (auto&& view : mSortedViews)
    {
        std::shared_ptr wrappedView = func(std::move(view));
        if (wrappedView)
        {
            result.mSortedViews.emplace_back(std::move(wrappedView));
        }
    }
    return result;
}
