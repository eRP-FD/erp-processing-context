/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX


#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <gsl/gsl-lite.hpp>
#include <rapidjson/fwd.h>
#include <map>
#include <ostream>

namespace fhirtools
{

struct KeyData {
    std::string_view environmentVariable;
    std::string_view jsonPath;
};

class FhirResourceViewConfiguration
{
public:
    explicit FhirResourceViewConfiguration(const gsl::not_null<const rapidjson::Value*>& fhirResourceTags,
                                           const gsl::not_null<const rapidjson::Value*>& fhirResourceViews);

    std::optional<std::string> getValidVersion(const std::string_view url, const Date& validDate) const;

    const std::vector<std::tuple<KeyData, std::string, std::string>>& variables() const;

private:
    struct ResourceView {
        std::optional<Date> mStart;
        std::optional<Date> mEnd;
        Version mVersion;

        bool isValidAt(const fhirtools::Date& date) const;
        bool overlaps(const ResourceView& other) const;
        bool isConsistent() const;
    };
    friend std::ostream& operator<<(std::ostream& os, const ResourceView& view);

    void check() const;
    void insert(const std::string& url, const ResourceView& resourceView);
    std::optional<Date> retrieveDateAndEnvVars(const std::string_view envName, const rapidjson::Pointer& pt,
                                               const rapidjson::Value& configItem);

    std::unordered_map<std::string, std::vector<ResourceView>> mResourceViews;
    std::vector<std::tuple<KeyData, std::string, std::string>> mVariables;
    std::vector<std::string> mEnvVars;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_CONFIGURATION_FHIRRESOURCEVIEWCONFIGURATION_HXX
