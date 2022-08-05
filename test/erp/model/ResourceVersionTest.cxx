/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ResourceVersion.hxx"
#include "erp/model/ModelException.hxx"
#include "fhirtools/model/Timestamp.hxx"
#include "erp/util/ErpException.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <memory>

using namespace ::std::chrono_literals;
using namespace ::std::literals::string_view_literals;

TEST(ResourceVersionTest, currentWrapper)
{
    const auto [gematikVersion, kbvVersion] = ::model::ResourceVersion::current();

    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>(),
              gematikVersion);
    EXPECT_EQ(::model::ResourceVersion::current<::model::ResourceVersion::KbvItaErp>(), kbvVersion);
}

TEST(ResourceVersionTest, currentSelectNewProfile)
{
    EnvironmentVariableGuard oldGematikVersion{"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK_VERSION", "1.0.3-1"};
    EnvironmentVariableGuard oldKbvVersion{"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_KBV_VERSION", "1.0.1"};
    EnvironmentVariableGuard newGematikVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION", "1.1.1"};
    EnvironmentVariableGuard newKbvVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_KBV_VERSION", "1.0.2"};

    const auto now = ::fhirtools::Timestamp::now().toXsDateTime();

    EnvironmentVariableGuard renderFrom{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    const auto [gematikVersion, kbvVersion] = ::model::ResourceVersion::current();

    ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1);
    ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_0_2);
}

TEST(ResourceVersionTest, currentSelectOldProfile)
{
    EnvironmentVariableGuard oldGematikVersion{"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK_VERSION", "1.0.3-1"};
    EnvironmentVariableGuard oldKbvVersion{"ERP_FHIR_PROFILE_OLD_XML_SCHEMA_KBV_VERSION", "1.0.1"};
    EnvironmentVariableGuard newGematikVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION", "1.1.1"};
    EnvironmentVariableGuard newKbvVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_KBV_VERSION", "1.0.2"};

    const auto future = (::fhirtools::Timestamp::now() + 24h).toXsDateTime();

    EnvironmentVariableGuard renderFrom{"ERP_FHIR_PROFILE_RENDER_FROM", future};

    const auto [gematikVersion, kbvVersion] = ::model::ResourceVersion::current();

    ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1);
    ASSERT_EQ(kbvVersion, ::model::ResourceVersion::KbvItaErp::v1_0_1);
}

TEST(ResourceVersionTest, versionizeProfile)//NOLINT(readability-function-cognitive-complexity)
{
    const auto fhirProfile = "http://hl7.org/fhir/StructureDefinition/Binary"sv;
    const auto gematikProfile = "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"sv;
    const auto kbvProfile = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Freetext"sv;
    const auto now = ::fhirtools::Timestamp::now().toXsDateTime();

    EnvironmentVariableGuard enableProfile{"ERP_FHIR_PROFILE_VALID_FROM", now};
    EnvironmentVariableGuard renderProfile{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    {
        ::std::unique_ptr<EnvironmentVariableGuard> forceGematikVersion1_1_1;
        ::std::unique_ptr<EnvironmentVariableGuard> forceKbvVersion1_0_2;
        if (::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>() ==
            ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1)
        {
            forceGematikVersion1_1_1 =
                ::std::make_unique<EnvironmentVariableGuard>("ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION", "1.1.1");
            forceKbvVersion1_0_2 =
                ::std::make_unique<EnvironmentVariableGuard>("ERP_FHIR_PROFILE_XML_SCHEMA_KBV_VERSION", "1.0.2");
        }

        const auto [gematikVersion, kbvVersion] = ::model::ResourceVersion::current();
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

    {
        // no version suffix for profile version 1.0.3-1 (backward compatibility)

        EnvironmentVariableGuard forceVersion1_0_3_1{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION", "1.0.3-1"};

        const auto gematikVersion =
            ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>();

        ASSERT_EQ(gematikVersion, ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(fhirProfile), fhirProfile);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(gematikProfile), gematikProfile);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(kbvProfile), kbvProfile);

        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|"), gematikProfile);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "||"), gematikProfile);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0"),
                  gematikProfile);
        EXPECT_EQ(::model::ResourceVersion::versionizeProfile(::std::string{gematikProfile} + "|1.0.0|"),
                  gematikProfile);
        EXPECT_THROW((void) ::model::ResourceVersion::versionizeProfile(""sv), ::std::logic_error);
        EXPECT_THROW((void) ::model::ResourceVersion::versionizeProfile("https://ibm.com/fhir/"sv), ::std::logic_error);
        EXPECT_THROW(
            (void) ::model::ResourceVersion::versionizeProfile(::std::string{"|1.0.0"} + ::std::string{gematikProfile}),
            ::std::logic_error);
        EXPECT_THROW(
            (void) ::model::ResourceVersion::versionizeProfile(::std::string{"1.0.0|"} + ::std::string{gematikProfile}),
            ::std::logic_error);
    }
}
