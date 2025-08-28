/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX


#include "KbvSchluesseltabellenConfiguration.hxx"
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
class FhirStructureRepositoryView;
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
        std::optional<date::local_days> mStart;
        std::optional<date::local_days> mEnd;
        std::set<std::string> mGroups;
        std::string mInfo;
        gsl::not_null<std::shared_ptr<FhirStructureRepositoryView>>
        view(gsl::not_null<const FhirStructureRepositoryBackend*> backend) const;
    };

    using ViewList = std::list<gsl::not_null<std::shared_ptr<const ViewConfig>>>;

    explicit FhirResourceViewConfiguration(const FhirResourceGroupResolver& resolver,
                                           const gsl::not_null<const rapidjson::Value*>& fhirResourceViews,
                                           const KbvSchluesseltabellenConfiguration& kbvSchluesseltabellenConfiguration,
                                           const date::days& globalOffset);

    ViewList getViewInfo(const date::local_days& validDate) const;
    ViewList getViewInfo(const Date& validDate) const;

    std::list<gsl::not_null<std::shared_ptr<const ViewConfig>>> allViews() const;
    const KbvSchluesseltabellenConfiguration& kbvSchluesseltabellenConfiguration() const;

    static std::tuple<std::optional<date::local_days>, std::optional<date::local_days>> mergePeriod(
        std::optional<date::local_days> viewStart, std::optional<date::local_days> viewEnd,
        std::optional<date::local_days> schluesselTabelleStart, std::optional<date::local_days> schluesselTabelleEnd);

private:
    class ViewConfigInternal : public ViewConfig
    {
    public:
        ViewConfigInternal(std::string id, std::optional<date::local_days> start, std::optional<date::local_days> end,
                           std::set<std::string> groups, std::string info);

        bool isConsistent() const;
    };

    friend std::ostream& operator<<(std::ostream& os, const ViewConfig& view);

    void addView(std::optional<date::local_days> start, std::optional<date::local_days> end,
                 const std::set<std::string>& groups, const std::string& id, const std::string& info);

    void check() const;

    KbvSchluesseltabellenConfiguration mKbvSchluesseltabellenConfiguration;
    std::multimap<date::local_days, std::shared_ptr<ViewConfigInternal>> mResourceViews;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
