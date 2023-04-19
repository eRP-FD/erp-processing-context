/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX

#include "erp/model/ResourceNames.hxx"
#include "erp/validation/SchemaType.hxx"

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
    v1_2_0,
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

std::optional<std::string> profileStr(SchemaType schemaType, model::ResourceVersion::FhirProfileBundleVersion version);


/**
 * @brief Given a profile uri, append the matching curently active (rendering) version
 *
 * @param profile Profile URL, may contain a version number, e.g. http://address|1.0
 * @return std::string the versioned profile, depending on the active bundle version
 */
[[nodiscard]] std::string versionizeProfile(std::string_view profile,
                                            FhirProfileBundleVersion bundleVersion = currentBundle());
[[nodiscard]] AllProfileVersion profileVersionFromBundle(FhirProfileBundleVersion bundleVersion);
template<typename SchemaVersionType>
[[nodiscard]] constexpr FhirProfileBundleVersion fhirProfileBundleFromSchemaVersion(SchemaVersionType schemaVersion);

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
bool deprecatedBundle(FhirProfileBundleVersion bundle);
template<typename SchemaVersionType>
constexpr bool deprecated(SchemaVersionType schemaVersion);


struct ProfileInfo {
    std::string_view profile{};
    std::string_view versionStr{};
    AnyProfileVersion version{};
    FhirProfileBundleVersion bundleVersion{};
    SchemaType schemaType{};
};

const ProfileInfo* profileInfoFromProfileName(std::string_view);

// The ProfileBundle template classes create a relation between
// * FhirProfileBundleVersion
// * Package specific Version enums (DeGematikErezeptWorkflowR4, KbvItaErp, ...)
// * profile uris
// * SchemaType
// as an example see: profileInfoFromProfileName(...)
template <FhirProfileBundleVersion, typename SchemaVersionType>
struct ProfileBundle;


template<>
struct ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, DeGematikErezeptWorkflowR4>{
    static constexpr DeGematikErezeptWorkflowR4 version = DeGematikErezeptWorkflowR4::v1_2_0;
    static constexpr std::string_view versionStr = "1.2";

    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {
        {SchemaType::Gem_erxAuditEvent, resource::structure_definition::auditEvent},
        {SchemaType::Gem_erxBinary,resource::structure_definition::binary},
        {SchemaType::Gem_erxCommunicationDispReq,resource::structure_definition::communicationDispReq},
        {SchemaType::Gem_erxCommunicationInfoReq,resource::structure_definition::communicationInfoReq},
        {SchemaType::Gem_erxCommunicationReply,resource::structure_definition::communicationReply},
        {SchemaType::Gem_erxCommunicationRepresentative,resource::structure_definition::communicationRepresentative},
        {SchemaType::Gem_erxCompositionElement,resource::structure_definition::composition},
        {SchemaType::Gem_erxDevice,resource::structure_definition::device},
        {SchemaType::Gem_erxMedicationDispense,resource::structure_definition::medicationDispense},
        {SchemaType::MedicationDispenseBundle, resource::structure_definition::medicationDispenseBundle},
        {SchemaType::Gem_erxReceiptBundle,resource::structure_definition::receipt},
        {SchemaType::Gem_erxTask,resource::structure_definition::task},
    };
};

template<>
struct ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, DeGematikErezeptWorkflowR4>{
    static constexpr DeGematikErezeptWorkflowR4 version = DeGematikErezeptWorkflowR4::v1_1_1;
    static constexpr std::string_view versionStr = "1.1.1";
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {
        {SchemaType::Gem_erxAuditEvent, resource::structure_definition::deprecated::auditEvent},
        {SchemaType::Gem_erxBinary,resource::structure_definition::deprecated::binary},
        {SchemaType::Gem_erxCommunicationDispReq,resource::structure_definition::deprecated::communicationDispReq},
        {SchemaType::Gem_erxCommunicationInfoReq,resource::structure_definition::deprecated::communicationInfoReq},
        {SchemaType::Gem_erxCommunicationReply,resource::structure_definition::deprecated::communicationReply},
        {SchemaType::Gem_erxCommunicationRepresentative,resource::structure_definition::deprecated::communicationRepresentative},
        {SchemaType::Gem_erxCompositionElement,resource::structure_definition::deprecated::composition},
        {SchemaType::Gem_erxDevice,resource::structure_definition::deprecated::device},
        {SchemaType::Gem_erxMedicationDispense,resource::structure_definition::deprecated::medicationDispense},
        {SchemaType::Gem_erxReceiptBundle,resource::structure_definition::deprecated::receipt},
        {SchemaType::Gem_erxTask,resource::structure_definition::deprecated::task},
    };
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, DeGematikErezeptPatientenrechnungR4>
{
    static constexpr DeGematikErezeptPatientenrechnungR4 version = DeGematikErezeptPatientenrechnungR4::v1_0_0;
    static constexpr std::string_view versionStr = "1.0";
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {
        {SchemaType::Gem_erxChargeItem, resource::structure_definition::chargeItem},
        {SchemaType::Gem_erxCommunicationChargChangeReq,resource::structure_definition::communicationChargChangeReq},
        {SchemaType::Gem_erxCommunicationChargChangeReply,resource::structure_definition::communicationChargChangeReply},
        {SchemaType::Gem_erxConsent, resource::structure_definition::consent},
    };
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, DeGematikErezeptPatientenrechnungR4>
{
    static constexpr DeGematikErezeptPatientenrechnungR4 version = DeGematikErezeptPatientenrechnungR4::invalid;
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {};
};


template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, KbvItaErp>
{
    static constexpr KbvItaErp version = KbvItaErp::v1_1_0;
    static constexpr std::string_view versionStr = "1.1.0";
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {
        {SchemaType::KBV_PR_ERP_Bundle, resource::structure_definition::prescriptionItem},
        {SchemaType::KBV_PR_ERP_Composition, resource::structure_definition::kbv_composition},
        {SchemaType::KBV_PR_ERP_Medication_Compounding,resource::structure_definition::kbv_medication_compounding},
        {SchemaType::KBV_PR_ERP_Medication_FreeText, resource::structure_definition::kbv_medication_free_text},
        {SchemaType::KBV_PR_ERP_Medication_Ingredient,resource::structure_definition::kbv_medication_ingredient},
        {SchemaType::KBV_PR_ERP_Medication_PZN, resource::structure_definition::kbv_medication_pzn},
        {SchemaType::KBV_PR_ERP_PracticeSupply,resource::structure_definition::kbv_practice_supply},
        {SchemaType::KBV_PR_ERP_Prescription, resource::structure_definition::kbv_medication_request},
    };
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, KbvItaErp>
    : ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, KbvItaErp>
{
    static constexpr KbvItaErp version = KbvItaErp::v1_0_2;
    static constexpr std::string_view versionStr = "1.0.2";
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, AbgabedatenPkv>
{
    static constexpr AbgabedatenPkv version = AbgabedatenPkv::v1_2_0;
    static constexpr std::string_view versionStr = "1.2";
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {
        {SchemaType::DAV_DispenseItem, resource::structure_definition::dispenseItem}

    };
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, AbgabedatenPkv>
{
    static constexpr AbgabedatenPkv version = AbgabedatenPkv::invalid;
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {};
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2023_07_01, NotProfiled>
{
    static constexpr NotProfiled version = {};
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {};
};

template <>
struct ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, NotProfiled>
{
    static constexpr NotProfiled version = {};
    static constexpr std::initializer_list<std::pair<SchemaType, std::string_view>> profileUris
    {};
};

template<typename SchemaVersionType>
constexpr bool deprecated(SchemaVersionType schemaVersion)
{
    return ProfileBundle<FhirProfileBundleVersion::v_2022_01_01, SchemaVersionType>::version == schemaVersion;
}

template<typename SchemaVersionType>
[[nodiscard]] constexpr FhirProfileBundleVersion fhirProfileBundleFromSchemaVersion(SchemaVersionType schemaVersion)
{
    using enum FhirProfileBundleVersion;
    if (ProfileBundle<v_2022_01_01, SchemaVersionType>::version == schemaVersion)
    {
        return v_2022_01_01;
    }
    return v_2023_07_01;
}

} // namespace ResourceVersion
} // namespace model

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
