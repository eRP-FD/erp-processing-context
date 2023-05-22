/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ResourceVersion.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/ErpException.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <memory>

using namespace ::std::chrono_literals;
using namespace ::std::literals::string_view_literals;

TEST(ResourceVersionTest, currentWrapper)
{
    const auto [gematikVersion, gematikPatientenRechnungVersion, kbvVersion, abdaPkvVersion, fhirVersion] =
        ::model::ResourceVersion::current();

    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>(),
              gematikVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptPatientenrechnungR4>(),
              gematikPatientenRechnungVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::KbvItaErp>(), kbvVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::Fhir>(), fhirVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::AbgabedatenPkv>(), abdaPkvVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::FhirProfileBundleVersion>(),
              ::model::ResourceVersion::currentBundle());
}

TEST(ResourceVersionTest, currentSelectNewProfile)
{
    EnvironmentVariableGuard oldBundledVersion{"ERP_FHIR_VERSION_OLD", "x"};
    EnvironmentVariableGuard newBundledVersion{"ERP_FHIR_VERSION", "2023.07.01"};

    const auto now = ::model::Timestamp::now().toXsDateTime();

    EnvironmentVariableGuard renderFrom{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    const auto [gematikVersion, gematikPatientenRechnungVersion, kbvVersion, abdaPkvVersion, fhirVersion] = ::model::ResourceVersion::current();
    const auto bundledVersion = ::model::ResourceVersion::currentBundle();

    ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0);
    ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_1_0);
    ASSERT_EQ(gematikPatientenRechnungVersion, ::model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0);
    ASSERT_EQ(fhirVersion, ::model::ResourceVersion::Fhir::v4_0_1);
    ASSERT_EQ(abdaPkvVersion, ::model::ResourceVersion::AbgabedatenPkv::v1_2_0);
    ASSERT_EQ(bundledVersion, ::model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01);
}

TEST(ResourceVersionTest, currentSelectOldProfile)
{
    auto envGuard = testutils::getOldFhirProfileEnvironment();

    const auto bundledVersion = ::model::ResourceVersion::currentBundle();
    ASSERT_EQ(bundledVersion, ::model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
}

TEST(ResourceVersionTest, versionizeProfile)//NOLINT(readability-function-cognitive-complexity)
{
    const auto fhirProfile = "http://hl7.org/fhir/StructureDefinition/Binary"sv;
    const auto gematikProfile = model::resource::structure_definition::medicationDispense;
    const auto kbvProfile = model::resource::structure_definition::kbcExForLegalBasis;

    {
        auto envGuard = testutils::getNewFhirProfileEnvironment();
        const auto [gematikVersion, gematikPatientenRechnungVersion, kbvVersion, abdaPkvVersion, fhirVersion] =
            model::ResourceVersion::current();
        const auto fhirProfileVersioned = ::std::string{fhirProfile} + "|4.0.1";
        const auto gematikProfileVersioned = ::std::string{gematikProfile} + ::std::string{"|"} +
                                             ::std::string{::model::ResourceVersion::v_str(gematikVersion)};
        const auto kbvProfileVersioned =
            std::string{kbvProfile} + std::string{"|"} + std::string{::model::ResourceVersion::v_str(kbvVersion)};

        ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0);
        ASSERT_EQ(gematikPatientenRechnungVersion, ::model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0);
        ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_1_0);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(fhirProfile), fhirProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(gematikProfile), gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(model::resource::structure_definition::markingFlag),
                  std::string{model::resource::structure_definition::markingFlag} + std::string("|1.0"));

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(fhirProfile), fhirProfileVersioned);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "||"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0|"),
                  gematikProfileVersioned);
        EXPECT_ANY_THROW((void) ::model::ResourceVersion::versionizeProfile(""sv));
        EXPECT_ANY_THROW((void) ::model::ResourceVersion::versionizeProfile("https://company.com/invalid/profile"sv));
        EXPECT_ANY_THROW((void) ::model::ResourceVersion::versionizeProfile(::std::string{"1.0.0|"} +
                                                                            ::std::string{gematikProfile}));
    }
}


TEST(ResourceVersionTest, supportedProfiles)
{
    {
        auto envGuards = testutils::getNewFhirProfileEnvironment();
        auto bundles = model::ResourceVersion::supportedBundles();
        ASSERT_EQ(bundles.size(), 1);
        ASSERT_TRUE(bundles.contains(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0));
        ASSERT_FALSE(
            model::ResourceVersion::isProfileSupported(model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1));
        ASSERT_FALSE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_0_2));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_1_0));
    }

    {
        auto envGuards = testutils::getOldFhirProfileEnvironment();
        auto bundles = model::ResourceVersion::supportedBundles();
        ASSERT_EQ(bundles.size(), 1);
        ASSERT_TRUE(bundles.contains(model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01));
        ASSERT_FALSE(model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0));
        ASSERT_TRUE(
            model::ResourceVersion::isProfileSupported(model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_0_2));
        ASSERT_FALSE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_1_0));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::invalid));
    }

    {
        auto envGuards = testutils::getOverlappingFhirProfileEnvironment();
        auto bundles = model::ResourceVersion::supportedBundles();
        ASSERT_EQ(bundles.size(), 2);
        ASSERT_TRUE(bundles.contains(model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01));
        ASSERT_TRUE(bundles.contains(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0));
        ASSERT_TRUE(
            model::ResourceVersion::isProfileSupported(model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(
            model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::invalid));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_0_2));
        ASSERT_TRUE(model::ResourceVersion::isProfileSupported(model::ResourceVersion::KbvItaErp::v1_1_0));
    }
}


TEST(ResourceVersionTest, profileVersionFromName)
{
    using namespace model::ResourceVersion;
    EXPECT_ANY_THROW((void)model::ResourceVersion::profileVersionFromName("http://example.com"));

    namespace structure_definition = model::resource::structure_definition;
    auto oldGematikProfile = structure_definition::deprecated::communicationDispReq;
    auto newGematikProfile = structure_definition::communicationChargChangeReq;
    auto oldKbvProfile = std::string{structure_definition::prescriptionItem} + "|" +
                         std::string{v_str(KbvItaErp::v1_0_2)};
    auto newKbvProfile =
        std::string{structure_definition::prescriptionItem} + "|" + std::string{v_str(KbvItaErp::v1_1_0)};
    auto unversionedFhir = std::string{structure_definition::communication};

    {
        auto envGuards = testutils::getNewFhirProfileEnvironment();
        EXPECT_THROW((void) profileVersionFromName(oldGematikProfile), ErpException);
        EXPECT_THROW((void) profileVersionFromName(oldKbvProfile), ErpException);
        EXPECT_EQ(profileVersionFromName(newGematikProfile),
                  std::make_tuple(AnyProfileVersion{DeGematikErezeptPatientenrechnungR4::v1_0_0},
                                  FhirProfileBundleVersion::v_2023_07_01));
        EXPECT_EQ(profileVersionFromName(newKbvProfile),
                  std::make_tuple(AnyProfileVersion{KbvItaErp::v1_1_0}, FhirProfileBundleVersion::v_2023_07_01));
        EXPECT_EQ(profileVersionFromName(unversionedFhir),
                  std::make_tuple(AnyProfileVersion{Fhir::v4_0_1}, FhirProfileBundleVersion::v_2023_07_01));
    }

    {
        auto envGuards = testutils::getOldFhirProfileEnvironment();
        EXPECT_THROW((void) profileVersionFromName(newGematikProfile), ErpException);
        EXPECT_THROW((void) profileVersionFromName(newKbvProfile), ErpException);
        EXPECT_EQ(profileVersionFromName(oldGematikProfile),
                  std::make_tuple(AnyProfileVersion{DeGematikErezeptWorkflowR4::v1_1_1},
                                  FhirProfileBundleVersion::v_2022_01_01));
        EXPECT_EQ(profileVersionFromName(oldKbvProfile),
                  std::make_tuple(AnyProfileVersion{KbvItaErp::v1_0_2}, FhirProfileBundleVersion::v_2022_01_01));
        EXPECT_EQ(profileVersionFromName(unversionedFhir),
                  std::make_tuple(AnyProfileVersion{Fhir::v4_0_1}, FhirProfileBundleVersion::v_2022_01_01));
    }

    {
        auto envGuards = testutils::getOverlappingFhirProfileEnvironment();
        EXPECT_EQ(profileVersionFromName(oldGematikProfile),
                  std::make_tuple(AnyProfileVersion{DeGematikErezeptWorkflowR4::v1_1_1},
                                  FhirProfileBundleVersion::v_2022_01_01));
        EXPECT_EQ(profileVersionFromName(newGematikProfile),
                  std::make_tuple(AnyProfileVersion{DeGematikErezeptPatientenrechnungR4::v1_0_0},
                                  FhirProfileBundleVersion::v_2023_07_01));
        EXPECT_EQ(profileVersionFromName(oldKbvProfile),
                  std::make_tuple(AnyProfileVersion{KbvItaErp::v1_0_2}, FhirProfileBundleVersion::v_2022_01_01));
        EXPECT_EQ(profileVersionFromName(newKbvProfile),
                  std::make_tuple(AnyProfileVersion{KbvItaErp::v1_1_0}, FhirProfileBundleVersion::v_2023_07_01));
        EXPECT_EQ(profileVersionFromName(unversionedFhir),
                  std::make_tuple(AnyProfileVersion{Fhir::v4_0_1}, FhirProfileBundleVersion::v_2023_07_01));
    }
}

struct ResourceVersionProfileStrTestParam {
    SchemaType schemaType{};
    model::ResourceVersion::FhirProfileBundleVersion bundle{};
    std::optional<std::string> expected;
};

class ResourceVersionProfileStrTest : public ::testing::TestWithParam<ResourceVersionProfileStrTestParam>
{
public:
    using TestWithParam::TestWithParam;
    static std::string name(const ::testing::TestParamInfo<ResourceVersionProfileStrTestParam>& paramInfo)
    {
        return std::string{magic_enum::enum_name(paramInfo.param.schemaType)}
            .append("_")
            .append(magic_enum::enum_name(paramInfo.param.bundle))
            .append(paramInfo.param.expected.has_value() ? "_defined" : "_undefined");
    }
};

TEST_P(ResourceVersionProfileStrTest, run)
{
    const auto& p = GetParam();
    auto profStr = model::ResourceVersion::profileStr(p.schemaType, p.bundle);
    ASSERT_EQ(profStr.has_value(), p.expected.has_value());
    if (p.expected.has_value())
    {
        EXPECT_EQ(*profStr, *p.expected);
    }
}
// clang-format off
using ProfBdlVer = model::ResourceVersion::FhirProfileBundleVersion;
INSTANTIATE_TEST_SUITE_P(all_profiled, ResourceVersionProfileStrTest, testing::ValuesIn<std::list<ResourceVersionProfileStrTestParam>>({
    {SchemaType::Gem_erxAuditEvent                    , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxAuditEvent|1.1.1"},
    {SchemaType::Gem_erxAuditEvent                    , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_AuditEvent|1.2"},
    {SchemaType::Gem_erxBinary                        , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxBinary|1.1.1"},
    {SchemaType::Gem_erxBinary                        , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Binary|1.2"},
    {SchemaType::Gem_erxCommunicationDispReq          , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq|1.1.1"},
    {SchemaType::Gem_erxCommunicationDispReq          , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq|1.2"},
    {SchemaType::Gem_erxCommunicationInfoReq          , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq|1.1.1"},
    {SchemaType::Gem_erxCommunicationInfoReq          , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_InfoReq|1.2"},
    {SchemaType::Gem_erxCommunicationReply            , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply|1.1.1"},
    {SchemaType::Gem_erxCommunicationReply            , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply|1.2"},
    {SchemaType::Gem_erxCommunicationRepresentative   , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative|1.1.1"},
    {SchemaType::Gem_erxCommunicationRepresentative   , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Representative|1.2"},
    {SchemaType::Gem_erxCompositionElement            , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxComposition|1.1.1"},
    {SchemaType::Gem_erxCompositionElement            , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Composition|1.2"},
    {SchemaType::Gem_erxDevice                        , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxDevice|1.1.1"},
    {SchemaType::Gem_erxDevice                        , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Device|1.2"},
    {SchemaType::KBV_PR_ERP_Bundle                    , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2"},
    {SchemaType::KBV_PR_ERP_Bundle                    , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0"},
    {SchemaType::KBV_PR_ERP_Composition               , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition|1.0.2"},
    {SchemaType::KBV_PR_ERP_Composition               , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition|1.1.0"},
    {SchemaType::KBV_PR_ERP_Medication_Compounding    , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding|1.0.2"},
    {SchemaType::KBV_PR_ERP_Medication_Compounding    , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding|1.1.0"},
    {SchemaType::KBV_PR_ERP_Medication_FreeText       , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText|1.0.2"},
    {SchemaType::KBV_PR_ERP_Medication_FreeText       , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText|1.1.0"},
    {SchemaType::KBV_PR_ERP_Medication_Ingredient     , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Ingredient|1.0.2"},
    {SchemaType::KBV_PR_ERP_Medication_Ingredient     , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Ingredient|1.1.0"},
    {SchemaType::KBV_PR_ERP_Medication_PZN            , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2"},
    {SchemaType::KBV_PR_ERP_Medication_PZN            , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.1.0"},
    {SchemaType::KBV_PR_ERP_PracticeSupply            , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_PracticeSupply|1.0.2"},
    {SchemaType::KBV_PR_ERP_PracticeSupply            , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_PracticeSupply|1.1.0"},
    {SchemaType::KBV_PR_ERP_Prescription              , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription|1.0.2"},
    {SchemaType::KBV_PR_ERP_Prescription              , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription|1.1.0"},
    // the KBV_PR_FOR resources appear as sub_resources only and therefore there is no SchemaVerionType for them:
    // {SchemaType::KBV_PR_FOR_Coverage                  , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Coverage|1.0.3"},
    // {SchemaType::KBV_PR_FOR_Coverage                  , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Coverage|1.1.0"},
    // {SchemaType::KBV_PR_FOR_Organization              , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Organization|1.0.3"},
    // {SchemaType::KBV_PR_FOR_Organization              , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Organization|1.1.0"},
    // {SchemaType::KBV_PR_FOR_Patient                   , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.0.3"},
    // {SchemaType::KBV_PR_FOR_Patient                   , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.1.0"},
    // {SchemaType::KBV_PR_FOR_Practitioner              , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Practitioner|1.0.3"},
    // {SchemaType::KBV_PR_FOR_Practitioner              , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Practitioner|1.1.0"},
    // {SchemaType::KBV_PR_FOR_PractitionerRole          , ProfBdlVer::v_2022_01_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_PractitionerRole|1.0.3"},
    // {SchemaType::KBV_PR_FOR_PractitionerRole          , ProfBdlVer::v_2023_07_01, "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_PractitionerRole|1.1.0"},
    {SchemaType::Gem_erxMedicationDispense            , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense|1.1.1"},
    {SchemaType::Gem_erxMedicationDispense            , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2"},
    {SchemaType::Gem_erxReceiptBundle                 , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxReceipt|1.1.1"},
    {SchemaType::Gem_erxReceiptBundle                 , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Bundle|1.2"},
    {SchemaType::Gem_erxTask                          , ProfBdlVer::v_2022_01_01, "https://gematik.de/fhir/StructureDefinition/ErxTask|1.1.1"},
    {SchemaType::Gem_erxTask                          , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task|1.2"},
}), &ResourceVersionProfileStrTest::name);

INSTANTIATE_TEST_SUITE_P(not_profiled, ResourceVersionProfileStrTest, testing::ValuesIn<std::list<ResourceVersionProfileStrTestParam>>({
    {SchemaType::PatchChargeItemParameters            , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::PatchChargeItemParameters            , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::ActivateTaskParameters               , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::ActivateTaskParameters               , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::CreateTaskParameters                 , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::CreateTaskParameters                 , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::KBV_PR_FOR_HumanName                 , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::KBV_PR_FOR_HumanName                 , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::fhir                                 , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::fhir                                 , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::BNA_tsl                              , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::BNA_tsl                              , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::Gematik_tsl                          , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Gematik_tsl                          , ProfBdlVer::v_2023_07_01, std::nullopt},
    {SchemaType::Pruefungsnachweis                    , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Pruefungsnachweis                    , ProfBdlVer::v_2023_07_01, std::nullopt},

}), &ResourceVersionProfileStrTest::name);
INSTANTIATE_TEST_SUITE_P(new_profiles, ResourceVersionProfileStrTest, testing::ValuesIn<std::list<ResourceVersionProfileStrTestParam>>({
    {SchemaType::Gem_erxChargeItem                    , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Gem_erxChargeItem                    , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_ChargeItem|1.0"},
    {SchemaType::Gem_erxConsent                       , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Gem_erxConsent                       , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Consent|1.0"},
    {SchemaType::DAV_DispenseItem                     , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::DAV_DispenseItem                     , ProfBdlVer::v_2023_07_01, "http://fhir.abda.de/eRezeptAbgabedaten/StructureDefinition/DAV-PKV-PR-ERP-AbgabedatenBundle|1.2"},
    {SchemaType::Gem_erxCommunicationChargChangeReq   , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Gem_erxCommunicationChargChangeReq   , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReq|1.0"},
    {SchemaType::Gem_erxCommunicationChargChangeReply , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::Gem_erxCommunicationChargChangeReply , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReply|1.0"},
    {SchemaType::MedicationDispenseBundle             , ProfBdlVer::v_2022_01_01, std::nullopt},
    {SchemaType::MedicationDispenseBundle             , ProfBdlVer::v_2023_07_01, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_CloseOperationInputBundle|1.2"},
}));

// clang-format on
