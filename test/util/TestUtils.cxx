/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/TestUtils.hxx"
#include "erp/model/Timestamp.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <date/tz.h>
#include <algorithm>
#include <chrono>
#include <regex>

namespace testutils
{

std::vector<EnvironmentVariableGuard> getNewFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto yesterday = (model::Timestamp::now() - date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    const auto dbYesterday = (model::Timestamp::now() - date::days{2}).toXsDate(model::Timestamp::GermanTimezone);
    const auto tomorrow = (model::Timestamp::now() + date::days{1}).toXsDate(model::Timestamp::GermanTimezone);

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, dbYesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM, tomorrow);
    return envVars;
}


std::vector<EnvironmentVariableGuard> getOldFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto tomorrow = (model::Timestamp::now() + date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    const auto today = (model::Timestamp::now()).toXsDate(model::Timestamp::GermanTimezone);

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, today);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM, tomorrow);
    return envVars;
}

std::vector<EnvironmentVariableGuard> getOverlappingFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto tomorrow = (model::Timestamp::now() + date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    const auto yesterday = (model::Timestamp::now() - date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM, tomorrow);
    return envVars;
}

std::vector<EnvironmentVariableGuard> getPatchedFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto yesterday = (model::Timestamp::now() - date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    const auto dbYesterday = (model::Timestamp::now() - date::days{2}).toXsDate(model::Timestamp::GermanTimezone);

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, dbYesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM, yesterday);
    return envVars;
}

std::string profile(SchemaType schemaType, model::ResourceVersion::FhirProfileBundleVersion version)
{
    using namespace std::string_literals;
    auto profStr = model::ResourceVersion::profileStr(schemaType, version);
    Expect3(profStr.has_value(),
            "cannot derive profile for: "s.append(magic_enum::enum_name(schemaType)).append("|").append(v_str(version)),
            std::logic_error);
    return *profStr;
}

std::string_view gkvKvid10(model::ResourceVersion::FhirProfileBundleVersion version)
{
    return deprecatedBundle(version)
        ?model::resource::naming_system::deprecated::gkvKvid10
        :model::resource::naming_system::gkvKvid10;
}

std::string_view prescriptionIdNamespace(model::ResourceVersion::FhirProfileBundleVersion version)
{
    return deprecatedBundle(version)
        ?model::resource::naming_system::deprecated::prescriptionID
        :model::resource::naming_system::prescriptionID;
}

std::string_view telematikIdNamespace(model::ResourceVersion::FhirProfileBundleVersion version)
{
    return deprecatedBundle(version)
        ?model::resource::naming_system::deprecated::telematicID
        :model::resource::naming_system::telematicID;
}

std::set<fhirtools::ValidationError> validationResultFilter(const fhirtools::ValidationResults& validationResult,
                                                            const fhirtools::ValidatorOptions& options)
{
    fhirtools::ValidationResults allowList;
    // TODO ERP-12940 begin: remove allowlist entries:
    allowList.add(options.levels.unreferencedBundledResource,
                  "reference must be resolvable: Binary/PrescriptionDigest-.+",
                  "Bundle.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[1].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[3].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[1].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[3].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    // TODO ERP-12940 end

    // allows description strings to partly match to be considered equal.
    auto customLess = [](const fhirtools::ValidationError& l, const fhirtools::ValidationError& r) {
        if (l.profile == r.profile && l.fieldName == r.fieldName && l.reason < r.reason)
        {
            if (l.reason.index() == r.reason.index() &&
                std::holds_alternative<fhirtools::ValidationError::MessageReason>(l.reason))
            {
                auto lr = std::get<fhirtools::ValidationError::MessageReason>(l.reason);
                auto rr = std::get<fhirtools::ValidationError::MessageReason>(r.reason);
                if (std::get<fhirtools::Severity>(lr) == std::get<fhirtools::Severity>(rr))
                {
                    auto lrr = std::get<std::string>(lr);
                    auto rrr = std::get<std::string>(rr);
                    // consider lr starts_with rr and vice versa as equals.
                    std::regex reg1(lrr);
                    std::regex reg2(rrr);
                    if (std::regex_match(rrr, reg1) || std::regex_match(lrr, reg2))
                    {
                        return false;
                    }
                }
            }
        }
        return l < r;
    };

    std::set<fhirtools::ValidationError> filteredValidationErrors;
    std::ranges::set_difference(validationResult.results(), allowList.results(),
                                std::inserter(filteredValidationErrors, filteredValidationErrors.begin()),
                                customLess);
    return filteredValidationErrors;
}

} // namespace testutils
