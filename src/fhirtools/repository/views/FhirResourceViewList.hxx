/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIRTOOLS_FHIRRESOURCEVIEWLIST_H
#define FHIRTOOLS_FHIRRESOURCEVIEWLIST_H

#include "FhirResourceViewConfiguration.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <functional>
#include <list>
#include <set>
#include <string>

namespace fhirtools
{

struct DefinitionKey;
class FhirStructureRepositoryBackend;
class FhirStructureRepositoryView;
class FhirVersion;

class FhirResourceViewList
{
public:
    using ViewPtr = gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>>;
    using ViewSource = std::function<ViewPtr(const FhirResourceViewConfiguration::ViewConfig&)>;

    FhirResourceViewList(const ViewSource& viewSource, const FhirResourceViewConfiguration::ViewList& viewListConfig);

    FhirResourceViewList(const FhirStructureRepositoryBackend& backend,
                         const FhirResourceViewConfiguration::ViewList& viewListConfig);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] const std::list<std::shared_ptr<const FhirStructureRepositoryView>>& all() const;
    [[nodiscard]] std::shared_ptr<const FhirStructureRepositoryView> latest() const;
    [[nodiscard]] std::shared_ptr<const FhirStructureRepositoryView> match(std::string url, FhirVersion version) const;
    [[nodiscard]] std::shared_ptr<const FhirStructureRepositoryView> match(DefinitionKey definitionKey) const;
    [[nodiscard]] FhirResourceViewList matchAll(const std::string& url, FhirVersion version) const;
    [[nodiscard]] std::set<DefinitionKey> supportedVersions(const std::list<std::string>& profileUrls) const;
    [[nodiscard]] DefinitionKey latestRenderVersion(const std::string& profileUrl) const;

    /// @brief transform all views
    ///
    /// @param func transformation function, if it returns nullptr the view will be discarded
    [[nodiscard]] FhirResourceViewList
    transform(const std::function<std::shared_ptr<const FhirStructureRepositoryView>(
                  gsl::not_null<std::shared_ptr<const FhirStructureRepositoryView>>)>& func) &&;

private:
    FhirResourceViewList();
    std::list<std::shared_ptr<const FhirStructureRepositoryView>> mSortedViews;
};

}

#endif// FHIRTOOLS_FHIRRESOURCEVIEWLIST_H
