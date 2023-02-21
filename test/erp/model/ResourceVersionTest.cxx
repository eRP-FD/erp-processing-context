/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
    ASSERT_EQ(abdaPkvVersion, ::model::ResourceVersion::AbgabedatenPkv::v1_1_0);
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
