/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX


#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <date/date.h>
#include <gsl/gsl-lite.hpp>
#include <rapidjson/fwd.h>
#include <future>
#include <iosfwd>
#include <list>
#include <map>
#include <set>
#include <vector>

namespace fhirtools
{
struct DefinitionKey;
class FhirResourceGroupResolver;
class FhirStructureRepository;
class FhirStructureRepositoryBackend;

struct KeyData {
    std::string environmentVariable;
    std::string jsonPath;
};

class FhirResourceViewConfiguration
{
public:
    class ViewConfig
    {
    public:
        std::string mId;
        std::string mEnvName;
        std::optional<date::local_days> mStart;
        std::optional<date::local_days> mEnd;
        std::set<std::string> mGroups;
        gsl::not_null<std::shared_ptr<FhirStructureRepository>>
        view(gsl::not_null<const FhirStructureRepositoryBackend*> backend) const;
    };

    class ViewList : public std::list<gsl::not_null<const std::shared_ptr<const ViewConfig>>>
    {
    public:
        const ViewConfig& latest() const;
        std::shared_ptr<FhirStructureRepository> match(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                                       const std::string& url, FhirVersion version) const;
        std::shared_ptr<FhirStructureRepository> match(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                                       const DefinitionKey& definitionKey) const;
        ViewList matchAll(const FhirStructureRepositoryBackend& backend, const std::string& url, FhirVersion version) const;
        std::set<DefinitionKey> supportedVersions(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                                   const std::list<std::string>& profileUrls) const;
    };

    explicit FhirResourceViewConfiguration(const FhirResourceGroupResolver& resolver,
                                           const gsl::not_null<const rapidjson::Value*>& fhirResourceViews,
                                           const date::days& globalOffset);

    ViewList getViewInfo(const date::local_days& validDate) const;
    ViewList getViewInfo(const Date& validDate) const;

    std::list<gsl::not_null<std::shared_ptr<const ViewConfig>>> allViews() const;


    const std::vector<std::tuple<KeyData, std::string, std::string>>& variables() const;

private:
    class ViewConfigInternal : public ViewConfig
    {
    public:
        ViewConfigInternal(std::string id, std::string envName, std::optional<date::local_days> start,
                           std::optional<date::local_days> end, std::set<std::string> groups);

        bool isConsistent() const;
    };

    friend std::ostream& operator<<(std::ostream& os, const ViewConfig& view);

    void check() const;
    std::optional<date::local_days> retrieveDateAndEnvVars(const std::string& envName, const rapidjson::Pointer& pt,
                                                           const rapidjson::Value& configItem);

    std::multimap<date::local_days, std::shared_ptr<ViewConfigInternal>> mResourceViews;
    std::vector<std::tuple<KeyData, std::string, std::string>> mVariables;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
