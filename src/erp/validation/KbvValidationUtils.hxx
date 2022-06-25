/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVVALIDATIONUTILS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVVALIDATIONUTILS_HXX

#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace model
{
class Extension;
class ResourceBase;
class KbvCoverage;
class UnspecifiedResource;
}

class KbvValidationUtils
{
public:
    static std::optional<model::Extension>
    checkGetExtension(const std::string_view& url, const model::ResourceBase& resource, bool mandatoryExtension);

    static std::string constraintError(const std::string& constraintMessage,
                                       const std::optional<std::string>& additional = std::nullopt);

    static void checkKbvExtensionValueCoding(const std::string_view& url, const model::ResourceBase& resource,
                                             bool mandatoryExtension, const std::set<std::string_view>& codeSystems,
                                             const std::set<std::string_view>& valueSet);
    static void checkKbvExtensionValueBoolean(const std::string_view& url, const model::ResourceBase& resource,
                                              bool mandatoryExtension);
    static void checkKbvExtensionValueString(const std::string_view& url, const model::ResourceBase& resource,
                                             bool mandatoryExtension,
                                             std::optional<size_t> maxStringLength = std::nullopt);
    static void checkKbvExtensionValueCode(const std::string_view& url, const model::ResourceBase& resource,
                                           bool mandatoryExtension, const std::set<std::string_view>& valueSet);
    static void checkKbvExtensionValueDate(const std::string_view& url, const model::ResourceBase& resource,
                                           bool mandatoryExtension);
    static void checkKbvExtensionValueRatio(const std::string_view& url, const model::ResourceBase& resource,
                                            bool mandatoryExtension);
    static void checkKbvExtensionValuePeriod(const std::string_view& url, const model::ResourceBase& resource,
                                             bool mandatoryExtension, bool valueMandatory);
    static void checkKbvExtensionValueIdentifier(const std::string_view& url, const model::ResourceBase& resource,
                                                 bool mandatoryExtension, const std::string_view& system,
                                                 size_t maxValueLength);

    static bool isKostentraeger(const model::KbvCoverage& coverage,
                                const std::set<std::string>& kostentraeger = {"GKV", "BG", "SKT", "UK"});

    static void checkIknr(const std::string_view& iknr);
    static void checkZanr(const std::string_view& zanr);
    static void checkKvnr(const std::string_view& kvnr);
    static void checkLanr(const std::string_view& lanr);

    static void checkFamilyName(const std::optional<model::UnspecifiedResource>& familyName);
    static void checkNamePrefix(const std::optional<model::UnspecifiedResource>& namePrefix);

    //constraints used by Patient and Practitioner names
    static void hum_1_2_3(const std::optional<model::UnspecifiedResource>& _family,
                          const std::optional<std::string_view>& family);
    static void hum_4(const std::optional<model::UnspecifiedResource>& _prefix,
                      const std::optional<std::string_view>& prefix);

    // constraints on address lines of Patient and Organization
    static void add1(const std::optional<std::string_view>& line,
                     const std::optional<model::UnspecifiedResource>& _line);
    static void add2(const std::optional<std::string_view>& line,
                     const std::optional<model::UnspecifiedResource>& _line);
    static void add3(const std::optional<std::string_view>& line,
                     const std::optional<model::UnspecifiedResource>& _line);
    static void add4(const std::optional<std::string_view>& line,
                     const std::optional<model::UnspecifiedResource>& _line, const std::string_view& type);
    static void add5(const std::optional<std::string_view>& line,
                     const std::optional<model::UnspecifiedResource>& _line);
    static void add6(const std::optional<model::UnspecifiedResource>& _line);
    static void add7(const model::UnspecifiedResource& address, const std::optional<std::string_view>& line,
                     const std::optional<std::string_view>& line2);
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVVALIDATIONUTILS_HXX
