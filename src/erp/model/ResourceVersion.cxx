/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/common/HttpStatus.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

using namespace ::std::literals::string_view_literals;

namespace model::ResourceVersion
{
static constexpr auto fhirProfileUriBase = "http://hl7.org/fhir/"sv;
static constexpr auto fhirProfileVersion = "4.0.1"sv;
static constexpr auto gematikProfileUriBase = "https://gematik.de"sv;
static constexpr auto kbvProfileUriBase = "https://fhir.kbv.de"sv;

std::string_view v_str(const DeGematikErezeptWorkflowR4& v)
{
    switch (v)
    {
        case DeGematikErezeptWorkflowR4::v1_0_3_1:
            return "1.0.3-1";
        case DeGematikErezeptWorkflowR4::v1_1_1:
            return "1.1.1";
    }
    Fail("Invalid de.gematik.erezept-workflow.r4 version: " + std::to_string(uintmax_t(v)));
}

std::string_view v_str(const KbvItaErp& v)
{
    switch (v)
    {
        case KbvItaErp::v1_0_1:
            return "1.0.1";
        case KbvItaErp::v1_0_2:
            return "1.0.2";
    }
    Fail("Invalid kbv.ita.erp version: " + std::to_string(uintmax_t(v)));
}

DeGematikErezeptWorkflowR4 str_vGematik(const std::string_view& versionString)
{
    static const std::unordered_map<std::string_view, DeGematikErezeptWorkflowR4> mapping{
        {"1.0.3-1", DeGematikErezeptWorkflowR4::v1_0_3_1},
        {"1.1.1", DeGematikErezeptWorkflowR4::v1_1_1}};
    auto canditate = mapping.find(versionString);
    ErpExpect(canditate != mapping.end(), HttpStatus::BadRequest,
              "invalid de.gematik.erezept-workflow.r4 version: " + std::string(versionString));
    return canditate->second;
}

KbvItaErp str_vKbv(const std::string_view& versionString)
{
    static const std::unordered_map<std::string_view, KbvItaErp> mapping{{"1.0.1", KbvItaErp::v1_0_1},
                                                                         {"1.0.2", KbvItaErp::v1_0_2}};
    auto canditate = mapping.find(versionString);
    ErpExpect(canditate != mapping.end(), HttpStatus::BadRequest,
              "invalid kbv.ita.erp version: " + std::string(versionString));
    return canditate->second;
}

std::tuple<DeGematikErezeptWorkflowR4, KbvItaErp> current()
{
    const auto& configuration = ::Configuration::instance();

    const auto newProfileRenderFromDateSetting =
        configuration.getOptionalStringValue(::ConfigurationKey::FHIR_PROFILE_RENDER_FROM);

    const auto now = ::model::Timestamp::now();
    if (newProfileRenderFromDateSetting &&
        (::model::Timestamp::fromXsDateTime(newProfileRenderFromDateSetting.value()) > now))
    {
        const auto gematikVersion = configuration.getOptionalStringValue(
            ::ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK_VERSION, "Unknown");

        const auto kbvVersion = configuration.getOptionalStringValue(
            ::ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_KBV_VERSION, "Unknown");

        return ::std::make_tuple(str_vGematik(gematikVersion), str_vKbv(kbvVersion));
    }

    const auto gematikVersion =
        configuration.getOptionalStringValue(::ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION, "Unknown");

    const auto kbvVersion =
        configuration.getOptionalStringValue(::ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_KBV_VERSION, "Unknown");

    return ::std::make_tuple(str_vGematik(gematikVersion), str_vKbv(kbvVersion));
}

::std::string versionizeProfile(::std::string_view profile)
{
    auto profileParts = ::String::split(profile, '|');
    Expect3(! profileParts.empty(), "Invalid profile selected.", ::std::logic_error);
    auto& profileValue = profileParts[0];
    const auto [gematikVersion, kbvVersion] = current();

    // omit the version for profile 1.0.3-1 to keep previous behaviour until switch to profile 1.1.0
    if (gematikVersion != DeGematikErezeptWorkflowR4::v1_0_3_1)
    {
        profileValue += "|";
        if (::String::starts_with(profile, gematikProfileUriBase))
        {
            profileValue += v_str(gematikVersion);
        }
        else if (::String::starts_with(profile, kbvProfileUriBase))
        {
            profileValue += v_str(kbvVersion);
        }
        else if (::String::starts_with(profile, fhirProfileUriBase))
        {
            profileValue += fhirProfileVersion;
        }
        else
        {
            Fail2("Invalid profile selected.", ::std::logic_error);
        }
    }
    else
    {
        // for behavioural consistency
        Expect3(::String::starts_with(profile, gematikProfileUriBase) ||
                    ::String::starts_with(profile, kbvProfileUriBase) ||
                    ::String::starts_with(profile, fhirProfileUriBase),
                "Invalid profile selected.", ::std::logic_error);
    }

    return profileValue;
}

}
