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

namespace
{
constexpr auto fhirProfileUriBase = "http://hl7.org/fhir/"sv;
constexpr auto fhirProfileVersion = "4.0.1"sv;
constexpr auto gematikChrgProfileUriBase = "https://gematik.de/fhir/erpchrg/"sv;
constexpr auto gematikErpProfileUriBase = "https://gematik.de/fhir/erp/"sv;
// obsolete as soon as DeGematikErezeptWorkflowR4::v1_1_1 is obsolete
constexpr auto gematikProfileUriBase = "https://gematik.de"sv;
constexpr auto kbvProfileUriBase = "https://fhir.kbv.de"sv;
constexpr auto davProfileUriBase = "http://fhir.abda.de/eRezeptAbgabedaten/"sv;

enum class ProfileType
{
    gematik,
    gematikDeprecated,
    gematikPatientenRechnung,
    kbv,
    fhir,
    Abda
};

ProfileType profileTypeFromName(std::string_view profileName)
{
    if (String::starts_with(profileName, gematikChrgProfileUriBase))
    {
        return ProfileType::gematikPatientenRechnung;
    }
    else if (String::starts_with(profileName, gematikErpProfileUriBase))
    {
        return ProfileType::gematik;
    }
    else if (String::starts_with(profileName, gematikProfileUriBase))
    {
        return ProfileType::gematikDeprecated;
    }
    else if (String::starts_with(profileName, kbvProfileUriBase))
    {
        return ProfileType::kbv;
    }
    else if (String::starts_with(profileName, fhirProfileUriBase))
    {
        return ProfileType::fhir;
    }
    else if (String::starts_with(profileName, davProfileUriBase))
    {
        return ProfileType::Abda;
    }
    ModelFail("Unable to determine profile type from name: " + std::string{profileName});
}

}// namespace

std::string_view v_str(DeGematikErezeptWorkflowR4 v)
{
    switch (v)
    {
        case DeGematikErezeptWorkflowR4::v1_1_1:
            return "1.1.1";
        case DeGematikErezeptWorkflowR4::v1_2_0:
            return "1.2";
    }
    Fail("Invalid de.gematik.erezept-workflow.r4 version: " + std::to_string(uintmax_t(v)));
}

std::string_view v_str(AbgabedatenPkv v)
{
    switch (v)
    {
        case AbgabedatenPkv::v1_2_0:
            return "1.2";
        case AbgabedatenPkv::invalid:
            Fail("Invalid AbgabedatenPkv version: 'invalid'");
    }
    Fail("Invalid AbgabedatenPkv version: " + std::to_string(uintmax_t(v)));
}

AbgabedatenPkv str_vAbdaPkv(std::string_view versionString)
{
    if ("1.2" == versionString)
        return AbgabedatenPkv::v1_2_0;
    Fail("Invalid AbgabedatenPkv version: " + std::string{versionString});
}

std::string_view v_str(Fhir /*v*/)
{
    return fhirProfileVersion;
}

Fhir str_vFhir(std::string_view versionString)
{
    if (fhirProfileVersion == versionString)
        return Fhir::v4_0_1;
    Fail("Invalid fhir version: " + std::string{versionString});
}

std::string_view v_str(KbvItaErp v)
{
    switch (v)
    {
        case KbvItaErp::v1_0_2:
            return "1.0.2";
        case KbvItaErp::v1_1_0:
            return "1.1.0";
    }
    Fail("Invalid kbv.ita.erp version: " + std::to_string(uintmax_t(v)));
}

DeGematikErezeptWorkflowR4 str_vGematik(std::string_view versionString)
{
    static const std::unordered_map<std::string_view, DeGematikErezeptWorkflowR4> mapping{
        {"2022.01.01", DeGematikErezeptWorkflowR4::v1_1_1},
        {"1.1.1", DeGematikErezeptWorkflowR4::v1_1_1},
        {"2023.07.01", DeGematikErezeptWorkflowR4::v1_2_0},
        {"1.2", DeGematikErezeptWorkflowR4::v1_2_0}};
    auto candidate = mapping.find(versionString);
    ErpExpect(candidate != mapping.end(), HttpStatus::BadRequest,
              "invalid de.gematik.erezept-workflow.r4 version: " + std::string(versionString));
    return candidate->second;
}

std::string_view v_str(DeGematikErezeptPatientenrechnungR4 v)
{
    switch (v)
    {
        case DeGematikErezeptPatientenrechnungR4::v1_0_0:
            return "1.0";
        case DeGematikErezeptPatientenrechnungR4::invalid:
            Fail("Invalid DeGematikErezeptPatientenrechnungR4 version: 'invalid'");
    }
    Fail("Invalid DeGematikErezeptPatientenrechnungR4 version: " + std::to_string(uintmax_t(v)));
}

DeGematikErezeptPatientenrechnungR4 str_vGematikPatientenRechnung(std::string_view versionString)
{
    static const std::unordered_map<std::string_view, DeGematikErezeptPatientenrechnungR4> mapping{
        {"1.0", DeGematikErezeptPatientenrechnungR4::v1_0_0}};
    auto candidate = mapping.find(versionString);
    ErpExpect(candidate != mapping.end(), HttpStatus::BadRequest,
              "invalid gematik.erezept-patientenrechnung.r4 version: " + std::string(versionString));
    return candidate->second;
}

KbvItaErp str_vKbv(std::string_view versionString)
{
    static const std::unordered_map<std::string_view, KbvItaErp> mapping{{"2022.01.01", KbvItaErp::v1_0_2},
                                                                         {"1.0.2", KbvItaErp::v1_0_2},
                                                                         {"2023.07.01", KbvItaErp::v1_1_0},
                                                                         {"1.1.0", KbvItaErp::v1_1_0}};
    auto candidate = mapping.find(versionString);
    ErpExpect(candidate != mapping.end(), HttpStatus::BadRequest,
              "invalid kbv.ita.erp version: " + std::string(versionString));
    return candidate->second;
}

std::string_view v_str(FhirProfileBundleVersion v)
{
    switch (v)
    {
        case FhirProfileBundleVersion::v_2022_01_01:
            return "2022.01.01";
        case FhirProfileBundleVersion::v_2023_07_01:
            return "2023.07.01";
    }
    Fail("Invalid FhirProfileBundleVersion version: " + std::to_string(uintmax_t(v)));
}

FhirProfileBundleVersion str_vBundled(std::string_view versionString)
{
    static const std::unordered_map<std::string_view, FhirProfileBundleVersion> mapping{
        {"2022.01.01", FhirProfileBundleVersion::v_2022_01_01},
        {"2023.07.01", FhirProfileBundleVersion::v_2023_07_01}};
    auto candidate = mapping.find(versionString);
    ErpExpect(candidate != mapping.end(), HttpStatus::BadRequest,
              "invalid FhirProfileBundleVersion version: " + std::string(versionString));
    return candidate->second;
}

AllProfileVersion profileVersionFromBundle(FhirProfileBundleVersion bundleVersion)
{
    static const std::unordered_map<FhirProfileBundleVersion, AllProfileVersion> mapping{
        {FhirProfileBundleVersion::v_2022_01_01,
         {DeGematikErezeptWorkflowR4::v1_1_1, DeGematikErezeptPatientenrechnungR4::invalid, KbvItaErp::v1_0_2,
         AbgabedatenPkv::invalid, Fhir::v4_0_1}},
        {FhirProfileBundleVersion::v_2023_07_01,
         {DeGematikErezeptWorkflowR4::v1_2_0, DeGematikErezeptPatientenrechnungR4::v1_0_0, KbvItaErp::v1_1_0,
         AbgabedatenPkv::v1_2_0, Fhir::v4_0_1}}};
    auto candidate = mapping.find(bundleVersion);
    ErpExpect(candidate != mapping.end(), HttpStatus::BadRequest,
              "Unknown bundle version: " + std::string(v_str(bundleVersion)));
    return candidate->second;
}


AllProfileVersion current()
{
    return profileVersionFromBundle(currentBundle());
}

AllProfileVersion deprecated()
{
    const auto& configuration = Configuration::instance();
    const auto oldBundleVersion = configuration.getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD);
    ErpExpect(oldBundleVersion.has_value(), HttpStatus::BadRequest, "Deprecated version is not defined");
    return profileVersionFromBundle(str_vBundled(*oldBundleVersion));
}

std::string_view v_str(AnyProfileVersion profileVersion)
{
    return std::visit(
        [](auto&& profileVersion) -> std::string_view {
            return v_str(profileVersion);
        },
        profileVersion);
}

std::optional<AnyProfileVersion> validateProfileVersionForBundle(ProfileType profileType,
                                                                 const std::optional<std::string>& versionNumberString,
                                                                 FhirProfileBundleVersion bundle)
{
    std::optional<AnyProfileVersion> version;
    auto validateProfile = [&versionNumberString](auto version, const auto& versionFromString) {
        return !versionNumberString.has_value() || versionFromString(*versionNumberString) == version;
    };
    auto [gematikVersion, gematikPatientenRechnungVersion, kbvVersion, abdaVersion, fhirVersion] =
        profileVersionFromBundle(bundle);
    switch (profileType)
    {
        case ProfileType::gematikDeprecated:
            if (validateProfile(gematikVersion, str_vGematik) && deprecatedProfile(gematikVersion))
            {
                version = gematikVersion;
            }
            break;
        case ProfileType::gematik:
            if (validateProfile(gematikVersion, str_vGematik) && ! deprecatedProfile(gematikVersion))
            {
                version = gematikVersion;
            }
            break;
        case ProfileType::gematikPatientenRechnung:
            if (validateProfile(gematikPatientenRechnungVersion, str_vGematikPatientenRechnung) &&
                gematikPatientenRechnungVersion != DeGematikErezeptPatientenrechnungR4::invalid)
            {
                version = gematikPatientenRechnungVersion;
            }
            break;
        case ProfileType::kbv:
            ErpExpect(versionNumberString, HttpStatus::BadRequest, "KBV resources must always define a profile version");
            if (validateProfile(kbvVersion, str_vKbv))
            {
                version = kbvVersion;
            }
            break;
        case ProfileType::fhir:
            if (validateProfile(fhirVersion, str_vFhir))
            {
                version = fhirVersion;
            }
            break;
        case ProfileType::Abda:
            if (validateProfile(abdaVersion, str_vAbdaPkv) && abdaVersion != AbgabedatenPkv::invalid)
            {
                version = abdaVersion;
            }
            break;
    }
    return version;
}

std::tuple<AnyProfileVersion, FhirProfileBundleVersion> profileVersionFromName(std::string_view profileName, const std::set<FhirProfileBundleVersion>& bundles)
{
    ProfileType profileType = profileTypeFromName(profileName);
    auto parts = String::split(std::string{profileName}, '|');
    std::optional<std::string> profileVersionString;
    ErpExpect(parts.size() <= 2, HttpStatus::BadRequest, "Invalid meta/profile: more than one | separator found.");
    if (parts.size() > 1)
    {
        profileVersionString = parts[1];
    }
    std::optional<std::tuple<AnyProfileVersion, FhirProfileBundleVersion>> version;
    for (const auto bundle : bundles)
    {
        auto profileVersion = validateProfileVersionForBundle(profileType, profileVersionString, bundle);
        if (profileVersion.has_value())
        {
            version.emplace(*profileVersion, bundle);
        }
    }
    ErpExpect(version.has_value(), HttpStatus::BadRequest, "Unsupported profile: " + std::string(profileName));
    return version.value();
}

FhirProfileBundleVersion currentBundle()
{
    const auto& configuration = ::Configuration::instance();

    const auto newProfileRenderFromDateSetting =
        configuration.getOptionalStringValue(::ConfigurationKey::FHIR_PROFILE_RENDER_FROM);

    const auto now = ::model::Timestamp::now();
    auto fhirVersionKey = ConfigurationKey::ERP_FHIR_VERSION;
    if (newProfileRenderFromDateSetting &&
        (::model::Timestamp::fromXsDateTime(newProfileRenderFromDateSetting.value()) > now))
    {
        fhirVersionKey = ConfigurationKey::ERP_FHIR_VERSION_OLD;
    }

    const auto bundledVersion = configuration.getOptionalStringValue(fhirVersionKey, "Unknown");
    return str_vBundled(bundledVersion);
}

std::set<FhirProfileBundleVersion> supportedBundles()
{
    std::set<FhirProfileBundleVersion> bundles;
    const auto& configuration = Configuration::instance();
    const auto now = Timestamp::now();
    auto newProfileValidFrom = configuration.getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_VALID_FROM);
    if (newProfileValidFrom.has_value() && now > Timestamp::fromXsDateTime(*newProfileValidFrom))
    {
        auto bundleVersion = configuration.getStringValue(ConfigurationKey::ERP_FHIR_VERSION);
        bundles.insert(str_vBundled(bundleVersion));
    }

    auto oldProfileValidUntil = configuration.getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL);
    if (oldProfileValidUntil.has_value() && now < Timestamp::fromXsDateTime(*oldProfileValidUntil))
    {
        auto bundleVersion = configuration.getStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD);
        bundles.insert(str_vBundled(bundleVersion));
    }

    ErpExpect(! bundles.empty(), HttpStatus::InternalServerError, "Empty set of supported bundles");
    return bundles;
}

std::set<FhirProfileBundleVersion> allBundles()
{
    std::set<FhirProfileBundleVersion> bundles;
    const auto& configuration = Configuration::instance();
    const auto oldBundleVersion = configuration.getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD);
    if (oldBundleVersion.has_value())
    {
        bundles.insert(str_vBundled(*oldBundleVersion));
    }
    const auto newBundleVersion = configuration.getStringValue(ConfigurationKey::ERP_FHIR_VERSION);
    bundles.insert(str_vBundled(newBundleVersion));
    return bundles;
}

std::optional<FhirProfileBundleVersion> isProfileSupported(AnyProfileVersion profileVersion,
                                                           const std::set<FhirProfileBundleVersion>& bundles)
{
    std::optional<FhirProfileBundleVersion> newestBundle;
    for (auto bundle : bundles)
    {
        auto supportedVersions = profileVersionFromBundle(bundle);
        bool supported = std::visit(
            [&supportedVersions](auto version) {
                return std::get<decltype(version)>(supportedVersions) == version;
            },
            profileVersion);

        if (supported &&
            (! newestBundle.has_value() || static_cast<int>(newestBundle.value()) < static_cast<int>(bundle)))
            newestBundle = bundle;
    }
    return newestBundle;
}


std::string versionizeProfile(std::string_view profile, FhirProfileBundleVersion bundleVersion)
{
    Expect(! profile.empty(), "profile must not be empty");
    auto profileName = String::split(profile, '|')[0];

    auto profileVersion = std::get<AnyProfileVersion>(profileVersionFromName(profileName, {bundleVersion}));

    profileName += "|";
    profileName += v_str(profileVersion);
    return profileName;
}

std::string_view v_str(WorkflowOrPatientenRechnungProfile profileVersion)
{
    if (std::holds_alternative<DeGematikErezeptWorkflowR4>(profileVersion))
    {
        return v_str(std::get<DeGematikErezeptWorkflowR4>(profileVersion));
    }
    return v_str(std::get<DeGematikErezeptPatientenrechnungR4>(profileVersion));
}

WorkflowOrPatientenRechnungProfile str_vWorkflowOrPatientenRechnung(std::string_view profile,
                                                                    std::string_view versionString)
{
    if (String::starts_with(profile, gematikChrgProfileUriBase))
    {
        return str_vGematikPatientenRechnung(versionString);
    }
    return str_vGematik(versionString);
}

std::string_view v_str(NotProfiled)
{
    return "";
}

bool deprecatedProfile(DeGematikErezeptWorkflowR4 profileVersion)
{
    return profileVersion == DeGematikErezeptWorkflowR4::v1_1_1;
}

bool deprecatedProfile(WorkflowOrPatientenRechnungProfile profileVersion)
{
    if (std::holds_alternative<ResourceVersion::DeGematikErezeptWorkflowR4>(profileVersion))
    {
        return deprecatedProfile(std::get<ResourceVersion::DeGematikErezeptWorkflowR4>(profileVersion));
    }
    return false;
}

bool deprecatedBundle(FhirProfileBundleVersion bundle)
{
    return bundle == FhirProfileBundleVersion::v_2022_01_01;
}


namespace
{
template <typename>
struct GetBundleVersion;

template <FhirProfileBundleVersion bundleVersionV, typename SchemaVersionType>
struct GetBundleVersion<ProfileBundle<bundleVersionV, SchemaVersionType>>
{
    static constexpr auto bundleVersion = bundleVersionV;
};

namespace ProfileFromSchemaTypeAndBundle
{
template<typename ProfileBundleT>
std::map<std::pair<SchemaType, ResourceVersion::FhirProfileBundleVersion>, std::string> makeProfileMap()
{
    if constexpr (ProfileBundleT::profileUris.size() > 0)
    {
        std::map<std::pair<SchemaType, model::ResourceVersion::FhirProfileBundleVersion>, std::string> result;
        for (const auto& uriInfo : ProfileBundleT::profileUris)
        {
            static constexpr auto bundleVersion = GetBundleVersion<ProfileBundleT>::bundleVersion;
            std::string profile{uriInfo.second};
            profile.append(1, '|').append(ProfileBundleT::versionStr);
            result.emplace(std::make_pair(uriInfo.first, bundleVersion), std::move(profile));
        }
        return result;
    }
    return {};
}

std::map<std::pair<SchemaType, ResourceVersion::FhirProfileBundleVersion>, std::string> makeProfileMap()
{
    using namespace model::ResourceVersion;
    using enum FhirProfileBundleVersion;
    std::map<std::pair<SchemaType, model::ResourceVersion::FhirProfileBundleVersion>, std::string> result;
    result.merge(makeProfileMap<ProfileBundle<v_2023_07_01, DeGematikErezeptWorkflowR4>>());
    result.merge(makeProfileMap<ProfileBundle<v_2022_01_01, DeGematikErezeptWorkflowR4>>());
    result.merge(makeProfileMap<ProfileBundle<v_2023_07_01, DeGematikErezeptPatientenrechnungR4>>());
    result.merge(makeProfileMap<ProfileBundle<v_2022_01_01, DeGematikErezeptPatientenrechnungR4>>());
    result.merge(makeProfileMap<ProfileBundle<v_2023_07_01, KbvItaErp>>());
    result.merge(makeProfileMap<ProfileBundle<v_2022_01_01, KbvItaErp>>());
    result.merge(makeProfileMap<ProfileBundle<v_2023_07_01, AbgabedatenPkv>>());
    result.merge(makeProfileMap<ProfileBundle<v_2022_01_01, AbgabedatenPkv>>());
    result.merge(makeProfileMap<ProfileBundle<v_2023_07_01, NotProfiled>>());
    result.merge(makeProfileMap<ProfileBundle<v_2022_01_01, NotProfiled>>());
    return result;
}
}// namespace ProfileFromSchemaTypeAndBundle
}// anonymous namespace

std::optional<std::string> profileStr(SchemaType schemaType, model::ResourceVersion::FhirProfileBundleVersion version)
{
    using namespace std::string_literals;
    static const auto profileMap = ProfileFromSchemaTypeAndBundle::makeProfileMap();
    auto prof = profileMap.find({schemaType, version});
    return prof == profileMap.end() ? std::nullopt : std::make_optional(prof->second);
}

namespace
{
namespace ProfileInfoFromProfileName
{
using ProfileUri = std::string_view;
using ProfileVersion = std::string_view;

template<typename ProfileBundleT>
std::map<std::pair<ProfileUri, ProfileVersion>, ProfileInfo> makeVersionMap()
{
    if constexpr (ProfileBundleT::profileUris.size() > 0)
    {
        std::map<std::pair<std::string_view, std::string_view>, ProfileInfo> result;
        ProfileInfo info;
        info.version = ProfileBundleT::version;
        info.versionStr = ProfileBundleT::versionStr;
        info.bundleVersion = GetBundleVersion<ProfileBundleT>::bundleVersion;
        for (const auto& uriInfo: ProfileBundleT::profileUris)
        {
            info.schemaType = uriInfo.first;
            info.profile = uriInfo.second;
            result.emplace(std::make_pair(uriInfo.second, ProfileBundleT::versionStr), info);
        }
        return result;
    }
    return {};
}

std::map<std::pair<ProfileUri, ProfileVersion>, ProfileInfo> makeVersionMap()
{
    using enum FhirProfileBundleVersion;
    std::map<std::pair<ProfileUri, ProfileVersion>, ProfileInfo> result;
    result.merge(makeVersionMap<ProfileBundle<v_2023_07_01, DeGematikErezeptWorkflowR4>>());
    result.merge(makeVersionMap<ProfileBundle<v_2022_01_01, DeGematikErezeptWorkflowR4>>());
    result.merge(makeVersionMap<ProfileBundle<v_2023_07_01, DeGematikErezeptPatientenrechnungR4>>());
    result.merge(makeVersionMap<ProfileBundle<v_2022_01_01, DeGematikErezeptPatientenrechnungR4>>());
    result.merge(makeVersionMap<ProfileBundle<v_2023_07_01, KbvItaErp>>());
    result.merge(makeVersionMap<ProfileBundle<v_2022_01_01, KbvItaErp>>());
    result.merge(makeVersionMap<ProfileBundle<v_2023_07_01, AbgabedatenPkv>>());
    result.merge(makeVersionMap<ProfileBundle<v_2022_01_01, AbgabedatenPkv>>());
    result.merge(makeVersionMap<ProfileBundle<v_2023_07_01, NotProfiled>>());
    result.merge(makeVersionMap<ProfileBundle<v_2022_01_01, NotProfiled>>());
    return result;
}
}// namespace ProfileInfoFromProfileName
}// anonymous namespace


const ProfileInfo* profileInfoFromProfileName(std::string_view profilename)
{
    static const auto versionMap = ProfileInfoFromProfileName::makeVersionMap();
    const auto pipe = profilename.rfind('|');
    if (pipe == std::string_view::npos || pipe >= profilename.size() - 1)
    {
        return nullptr;
    }
    const auto profileUri = profilename.substr(0, pipe);
    const auto version = profilename.substr(pipe + 1);
    const auto info = versionMap.find(std::make_pair(profileUri, version));
    return (info != versionMap.end())?std::addressof(info->second):nullptr;
}

}// namespace model::ResourceVersion
