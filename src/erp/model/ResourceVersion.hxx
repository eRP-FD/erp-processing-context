/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

namespace model
{
namespace ResourceVersion
{
enum class NotProfiled {};

enum class DeGematikErezeptWorkflowR4
{
    v1_1_1,// https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.1.1
    v1_2_0,// https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.2.0
};
std::string_view v_str(DeGematikErezeptWorkflowR4 v);
DeGematikErezeptWorkflowR4 str_vGematik(std::string_view versionString);

enum class AbgabedatenPkv
{
    invalid,
    v1_1_0,
};
std::string_view v_str(AbgabedatenPkv v);
AbgabedatenPkv str_vAbdaPkv(std::string_view versionString);

enum class DeGematikErezeptPatientenrechnungR4
{
    invalid,
    v1_0_0 // https://simplifier.net/packages/de.gematik.erezept-patientenrechnung.r4/1.0.0
};
std::string_view v_str(DeGematikErezeptPatientenrechnungR4 v);
DeGematikErezeptPatientenrechnungR4 str_vGematikPatientenRechnung(std::string_view versionString);

enum class KbvItaErp
{
    v1_0_2,// https://simplifier.net/packages/kbv.ita.erp/1.0.2
    v1_1_0,// https://simplifier.net/packages/kbv.ita.erp/1.1.0
};

std::string_view v_str(KbvItaErp v);
KbvItaErp str_vKbv(std::string_view versionString);

enum class Fhir
{
    v4_0_1
};

std::string_view v_str(Fhir v);
Fhir str_vFhir(std::string_view versionString);

// Bundled profile versions of Gematik, KBV, DAV, PKV, GKV profiles.
// See https://github.com/gematik/api-erp/blob/master/docs/erp_fhirversion.adoc
enum class FhirProfileBundleVersion
{
    /**
     * KBV: 1.0.2
     * gematik: 1.1.1
     * DAV: Package 1.1.2 Profile 1.1.0
     * GKV: 1.1.0
     * PKV: 1.1.0-rc6
     */
    v_2022_01_01,

    /**
     * KBV: 1.1.0
     * gematik: Package 1.2.0 Profile 1.2
     * gematik patientenrechnung: 1.0.0-rc3
     * DAV: Package 1.2.0 Profile 1.2
     * GKV: Package 1.2.0 Profile 1.2
     * PKV: 1.1.0-rc10
     */
    v_2023_07_01
};

FhirProfileBundleVersion currentBundle();

using AnyProfileVersion = std::variant<DeGematikErezeptWorkflowR4, DeGematikErezeptPatientenrechnungR4, KbvItaErp, AbgabedatenPkv, Fhir>;
using AllProfileVersion = std::tuple<DeGematikErezeptWorkflowR4, DeGematikErezeptPatientenrechnungR4, KbvItaErp, AbgabedatenPkv, Fhir>;
using WorkflowOrPatientenRechnungProfile = std::variant<std::monostate, DeGematikErezeptPatientenrechnungR4, DeGematikErezeptWorkflowR4>;

std::string_view v_str(FhirProfileBundleVersion v);
FhirProfileBundleVersion str_vBundled(std::string_view versionString);
std::string_view v_str(WorkflowOrPatientenRechnungProfile profileVersion);
WorkflowOrPatientenRechnungProfile str_vWorkflowOrPatientenRechnung(std::string_view profile, std::string_view versionString);
std::string_view v_str(AnyProfileVersion profileVersion);

/**
 * @brief Given a profile uri, append the matching curently active (rendering) version
 *
 * @param profile Profile URL, may contain a version number, e.g. http://address|1.0
 * @return std::string the versioned profile, depending on the active bundle version
 */
[[nodiscard]] std::string versionizeProfile(std::string_view profile);
[[nodiscard]] AllProfileVersion profileVersionFromBundle(FhirProfileBundleVersion bundleVersion);

/**
 * @brief Get each profile version from the current active (rendering) bundle
 *
 * @return AllProfileVersion
 */
[[nodiscard]] AllProfileVersion current();

/**
 * @brief Get each profile version from the deprecated bundle
 *
 * @return AllProfileVersion
 */
[[nodiscard]] AllProfileVersion deprecated();

/**
 * Get the currently supported profile bundles, depending on configuration and the current time
 */
[[nodiscard]] std::set<FhirProfileBundleVersion> supportedBundles();

/**
 * Get all available profile bundles, regardless of the current configuration.
 */
[[nodiscard]] std::set<FhirProfileBundleVersion> allBundles();
/**
 * @brief Determine the profile version and the bundle based on the name of the profile.
 * @param profileName Name of the profile
 * @param bundles The profiles bundles to take into consideration when determining the profile version.
 * @throws ErpException if the profile is not supported in the given bundles, std::logic_error if the
 * profile type could not be determined from the name
 */
[[nodiscard]] std::tuple<AnyProfileVersion, FhirProfileBundleVersion>
profileVersionFromName(std::string_view profileName,
                       const std::set<FhirProfileBundleVersion>& bundles = supportedBundles());

/**
 * @brief Determine if the given versioned profile is supported by any of the active bundles.
 *
 * @param profileVersion Versioned name of the profile
 * @param bundles the bundles to test
 * @return true, if any bundle contains the given profile
 */
std::optional<FhirProfileBundleVersion>
isProfileSupported(AnyProfileVersion profileVersion,
                   const std::set<FhirProfileBundleVersion>& bundles = supportedBundles());

template<typename VersionType>
[[nodiscard]] inline
VersionType current()
{
    return std::get<VersionType>(current());
}

template<typename VersionType>
[[nodiscard]] inline
VersionType deprecated()
{
    return std::get<VersionType>(deprecated());
}

template<>
[[nodiscard]] inline
WorkflowOrPatientenRechnungProfile deprecated()
{
    return std::get<DeGematikErezeptWorkflowR4>(deprecated());
}

template<>
[[nodiscard]] inline
NotProfiled deprecated()
{
    return {};
}


template<>
[[nodiscard]] inline
FhirProfileBundleVersion current()
{
    return currentBundle();
}

std::string_view v_str(NotProfiled);

bool deprecatedProfile(DeGematikErezeptWorkflowR4 profileVersion);
bool deprecatedProfile(WorkflowOrPatientenRechnungProfile profileVersion);

} // namespace ResourceVersion
} // namespace model

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
