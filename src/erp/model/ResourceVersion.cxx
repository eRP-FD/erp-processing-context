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
        case DeGematikErezeptWorkflowR4::v1_1_1:
            return "1.1.1";
    }
    Fail("Invalid de.gematik.erezept-workflow.r4 version: " + std::to_string(uintmax_t(v)));
}

std::string_view v_str(const KbvItaErp& v)
{
    switch (v)
    {
        case KbvItaErp::v1_0_2:
            return "1.0.2";
    }
    Fail("Invalid kbv.ita.erp version: " + std::to_string(uintmax_t(v)));
}

DeGematikErezeptWorkflowR4 str_vGematik(const std::string_view& versionString)
{
    static const std::unordered_map<std::string_view, DeGematikErezeptWorkflowR4> mapping{
        {"2022.01.01", DeGematikErezeptWorkflowR4::v1_1_1},
        {"1.1.1", DeGematikErezeptWorkflowR4::v1_1_1}};
    auto canditate = mapping.find(versionString);
    ErpExpect(canditate != mapping.end(), HttpStatus::BadRequest,
              "invalid de.gematik.erezept-workflow.r4 version: " + std::string(versionString));
    return canditate->second;
}

KbvItaErp str_vKbv(const std::string_view& versionString)
{
    static const std::unordered_map<std::string_view, KbvItaErp> mapping{{"2022.01.01", KbvItaErp::v1_0_2},
                                                                         {"1.0.2", KbvItaErp::v1_0_2}};
    auto canditate = mapping.find(versionString);
    ErpExpect(canditate != mapping.end(), HttpStatus::BadRequest,
              "invalid kbv.ita.erp version: " + std::string(versionString));
    return canditate->second;
}

std::string_view v_str(const FhirProfileBundleVersion& v)
{
    switch (v)
    {
        case FhirProfileBundleVersion::v_2022_01_01:
            return "2022.01.01";
        case FhirProfileBundleVersion::v_2022_07_01:
            return "2022.07.01";
        case FhirProfileBundleVersion::v_2023_01_01:
            return "2023.01.01";
    }
    Fail("Invalid FhirProfileBundleVersion version: " + std::to_string(uintmax_t(v)));
}

FhirProfileBundleVersion str_vBundled(const std::string_view& versionString)
{
    static const std::unordered_map<std::string_view, FhirProfileBundleVersion> mapping{
        {"2022.01.01", FhirProfileBundleVersion::v_2022_01_01},
        {"2022.07.01", FhirProfileBundleVersion::v_2022_07_01},
        {"2023.01.01", FhirProfileBundleVersion::v_2023_01_01}};
    auto canditate = mapping.find(versionString);
    ErpExpect(canditate != mapping.end(), HttpStatus::BadRequest,
              "invalid FhirProfileBundleVersion version: " + std::string(versionString));
    return canditate->second;
}

std::tuple<DeGematikErezeptWorkflowR4, KbvItaErp, FhirProfileBundleVersion> current()
{
    const auto& configuration = ::Configuration::instance();

    const auto newProfileRenderFromDateSetting =
        configuration.getOptionalStringValue(::ConfigurationKey::FHIR_PROFILE_RENDER_FROM);

    const auto now = ::model::Timestamp::now();
    if (newProfileRenderFromDateSetting &&
        (::model::Timestamp::fromXsDateTime(newProfileRenderFromDateSetting.value()) > now))
    {
        const auto bundledVersion =
            configuration.getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD, "Unknown");

        return ::std::make_tuple(str_vGematik(bundledVersion), str_vKbv(bundledVersion), str_vBundled(bundledVersion));
    }

    const auto bundledVersion = configuration.getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION, "Unknown");

    return ::std::make_tuple(str_vGematik(bundledVersion), str_vKbv(bundledVersion), str_vBundled(bundledVersion));
}

::std::string versionizeProfile(::std::string_view profile)
{
    auto profileParts = String::split(profile, '|');
    Expect3(! profileParts.empty(), "Invalid profile selected.", ::std::logic_error);
    auto& profileValue = profileParts[0];
    const auto [gematikVersion, kbvVersion, bundledVersion] = current();

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

    return profileValue;
}

std::string_view v_str(const NotProfiled&)
{
    return "";
}

}
