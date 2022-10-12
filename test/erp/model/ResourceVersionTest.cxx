/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ResourceVersion.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/ErpException.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <memory>

using namespace ::std::chrono_literals;
using namespace ::std::literals::string_view_literals;

TEST(ResourceVersionTest, currentWrapper)
{
    const auto [gematikVersion, kbvVersion, bundledVersion] = ::model::ResourceVersion::current();

    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>(),
              gematikVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::KbvItaErp>(), kbvVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::FhirProfileBundleVersion>(), bundledVersion);
}

TEST(ResourceVersionTest, currentSelectNewProfile)
{
    EnvironmentVariableGuard oldBundledVersion{"ERP_FHIR_VERSION_OLD", "x"};
    EnvironmentVariableGuard newBundledVersion{"ERP_FHIR_VERSION", "2022.01.01"};

    const auto now = ::model::Timestamp::now().toXsDateTime();

    EnvironmentVariableGuard renderFrom{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    const auto [gematikVersion, kbvVersion, bundledVersion] = ::model::ResourceVersion::current();

    ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1);
    ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_0_2);
    ASSERT_EQ(bundledVersion, ::model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
}

TEST(ResourceVersionTest, currentSelectOldProfile)
{
    EnvironmentVariableGuard oldBundledVersion{"ERP_FHIR_VERSION_OLD", "2022.01.01"};
    EnvironmentVariableGuard newBundledVersion{"ERP_FHIR_VERSION", "2022.07.01"};

    const auto future = (::model::Timestamp::now() + 24h).toXsDateTime();

    EnvironmentVariableGuard renderFrom{"ERP_FHIR_PROFILE_RENDER_FROM", future};

    const auto [gematikVersion, kbvVersion, bundledVersion] = ::model::ResourceVersion::current();

    ASSERT_EQ(bundledVersion, ::model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
}

TEST(ResourceVersionTest, versionizeProfile)//NOLINT(readability-function-cognitive-complexity)
{
    const auto fhirProfile = "http://hl7.org/fhir/StructureDefinition/Binary"sv;
    const auto gematikProfile = "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"sv;
    const auto kbvProfile = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Freetext"sv;
    const auto now = ::model::Timestamp::now().toXsDateTime();

    EnvironmentVariableGuard enableProfile{"ERP_FHIR_PROFILE_VALID_FROM", now};
    EnvironmentVariableGuard renderProfile{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    {
        ::std::unique_ptr<EnvironmentVariableGuard> forceGematikVersion1_1_1;
        ::std::unique_ptr<EnvironmentVariableGuard> forceKbvVersion1_0_2;

        const auto [gematikVersion, kbvVersion, _] = ::model::ResourceVersion::current();
        const auto fhirProfileVersioned = ::std::string{fhirProfile} + "|4.0.1";
        const auto gematikProfileVersioned = ::std::string{gematikProfile} + ::std::string{"|"} +
                                             ::std::string{::model::ResourceVersion::v_str(gematikVersion)};
        const auto kbvProfileVersioned =
            ::std::string{kbvProfile} + ::std::string{"|"} + ::std::string{::model::ResourceVersion::v_str(kbvVersion)};

        ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1);
        ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_0_2);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(fhirProfile), fhirProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(gematikProfile), gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(kbvProfile), kbvProfileVersioned);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "||"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0"),
                  gematikProfileVersioned);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0|"),
                  gematikProfileVersioned);
        EXPECT_THROW((void) ::model::ResourceVersion::versionizeProfile(""sv), ::std::logic_error);
        EXPECT_THROW((void) ::model::ResourceVersion::versionizeProfile("https://company.com/invalid/profile"sv),
                     ::std::logic_error);
        EXPECT_THROW(
            (void) ::model::ResourceVersion::versionizeProfile(::std::string{"|1.0.0"} + ::std::string{gematikProfile}),
            ::std::logic_error);
        EXPECT_THROW(
            (void) ::model::ResourceVersion::versionizeProfile(::std::string{"1.0.0|"} + ::std::string{gematikProfile}),
            ::std::logic_error);
    }
}
