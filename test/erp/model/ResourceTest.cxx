/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/String.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

#include <array>
#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

using namespace ::std::literals::string_view_literals;

namespace model
{

class FriendlyResourceBase : public ResourceBase
{
    using ResourceBase::ResourceBase;

    FRIEND_TEST(ResourceBaseTest, Constructor);
};

TEST(ResourceBaseTest, Constructor)//NOLINT(readability-function-cognitive-complexity)
{
    const auto envGuards = testutils::getOverlappingFhirProfileEnvironment();
    const auto [oldGematikVersion, oldPatientenRechnungVersion, oldKbvVersion, oldAbdaPkvVersion, oldFhirVersion] =
        ResourceVersion::profileVersionFromBundle(ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
    const auto [newGematikVersion, newPatientenRechnungVersion, newKbvVersion, newAbdaPkvVersion, newFhirVersion] =
        ResourceVersion::profileVersionFromBundle(ResourceVersion::FhirProfileBundleVersion::v_2023_07_01);

    const std::array<std::tuple<std::string_view, std::string_view, bool>, 3> profileTypes = {
        // only supported by old profile
        std::make_tuple(resource::structure_definition::deprecated::receipt, ResourceVersion::v_str(oldGematikVersion), false),
        // only supported by new profiles
        std::make_tuple(resource::structure_definition::receipt, ResourceVersion::v_str(newGematikVersion), true),
        // a generic fhir profile
        std::make_tuple(resource::structure_definition::deprecated::digest, ResourceVersion::v_str(newFhirVersion), true)};

    //NOLINTBEGIN(cppcoreguidelines-owning-memory)
    std::unique_ptr<FriendlyResourceBase> resource;
    for (const auto& type : profileTypes)
    {
        auto env = get<2>(type)?testutils::getNewFhirProfileEnvironment():testutils::getOldFhirProfileEnvironment();
        ASSERT_NO_THROW(resource.reset(new FriendlyResourceBase{get<0>(type)}));
        const auto profile = resource->getOptionalStringValue(::rapidjson::Pointer{"/meta/profile/0"});
        ASSERT_TRUE(profile);
        const auto profileParts = String::split(profile.value(), '|');
        ASSERT_EQ(profileParts.size(), 2);
        EXPECT_EQ(profileParts[0], get<0>(type));
        EXPECT_EQ(profileParts[1], get<1>(type)) << "Profile = " << profileParts[0];
    }

    {
        EXPECT_ANY_THROW(resource.reset(new FriendlyResourceBase{""}));
        EXPECT_ANY_THROW(resource.reset(new FriendlyResourceBase{"https://company.com/invalid/profile"}));
    }
    //NOLINTEND(cppcoreguidelines-owning-memory)
}
} // namespace model

class ResourceGenericValidationTest : public ::testing::TestWithParam<Configuration::GenericValidationMode>
{
public:
    static bool oldProfilesAllowed()
    {
        namespace RV = model::ResourceVersion;
        return RV::supportedBundles().contains(RV::FhirProfileBundleVersion::v_2022_01_01);
    }

    void SetUp() override
    {
        switch (GetParam())
        {
            using enum Configuration::GenericValidationMode;
            case require_success:
                break;
            // these options have no meaning without XSD or In-Code validation:
            case disable:
            case detail_only:
            case ignore_errors:
                if (! oldProfilesAllowed())
                {
                    GTEST_SKIP();
                }
        }
    }
protected:
    template <typename ResourceT>
    ResourceT validated(model::ResourceFactory<ResourceT>&& factory, SchemaType schemaType)
    {
        return std::move(factory).getValidated(schemaType, *StaticData::getXmlValidator(),*StaticData::getInCodeValidator());
    }

    template<typename ResourceT = model::Parameters>
    ResourceT fromXml(std::string_view xml, SchemaType schemaType = SchemaType::fhir,
                      const fhirtools::ValidatorOptions& valOpts = {})
    {
        return validated(model::ResourceFactory<ResourceT>::fromXml(xml, *StaticData::getXmlValidator(),
                                                                    {.validatorOptions = valOpts}), schemaType);
    }

    template<typename ResourceT = model::Parameters>
    ResourceT fromJson(std::string_view json, SchemaType schemaType = SchemaType::fhir,
                       const fhirtools::ValidatorOptions& valOpts = {})
    {
        return validated(model::ResourceFactory<ResourceT>::fromJson(json, *StaticData::getJsonValidator(),
                                                                     {.validatorOptions = valOpts}), schemaType);
    }

    std::string goodParametersXML() const
    {
        return resourceManager.getStringResource("test/validation/xml/parameters/simpleParameters_valid.xml");
    }
    std::string genericFailParametersXML() const
    {
        return resourceManager.getStringResource(
            "test/validation/xml/parameters/simpleParameters_invalid_emptyExtension.xml");
    }
    std::string badParametersXML() const
    {
        return resourceManager.getStringResource(
            "test/validation/xml/parameters/simpleParameters_invalid_noUrlInExtension.xml");
    }
    std::string goodParametersJSON() const
    {
        return resourceManager.getStringResource("test/validation/xml/parameters/simpleParameters_valid.json");
    }
    std::string genericFailParametersJSON() const
    {
        return resourceManager.getStringResource(
            "test/validation/xml/parameters/simpleParameters_invalid_emptyExtension.json");
    }
    std::string badParametersJSON() const
    {
        return resourceManager.getStringResource(
            "test/validation/xml/parameters/simpleParameters_invalid_noUrlInExtension.json");
    }

    std::string goodBundleXML()
    {
        return ResourceTemplates::kbvBundleXml();
    }
    std::string genericFailBundleXML()
    {
        const char* file = oldProfilesAllowed()?"/kbv_bundle_duplicate_fullUrl.xml"
                                               :"/kbv_bundle_1.1.0_duplicate_organization.xml";
        return resourceManager.getStringResource(dataPath + file);
    }

    std::string badBundleXML()
    {
        const char* file = oldProfilesAllowed()?"/kbv_bundle_duplicate_fullUrl_missing_code.xml"
                                               :"/kbv_bundle_1.1.0_duplicate_organization.xml";
        return resourceManager.getStringResource(dataPath + file);
    }

    std::string goodBundleJSON()
    {
        return Fhir::instance().converter().xmlStringToJson(goodBundleXML()).serializeToJsonString();
    }
    std::string genericFailBundleJSON()
    {
        return Fhir::instance().converter().xmlStringToJson(genericFailBundleXML()).serializeToJsonString();
    }
    std::string badBundleJSON()
    {
        return Fhir::instance().converter().xmlStringToJson(badBundleXML()).serializeToJsonString();
    }


    template<typename T>
    void withDiagnosticCheck(T func, const std::string& message, const std::string& shortDiagnostics,
                             const std::string& fullDiagnostics)
    {
        try
        {
            ASSERT_NO_FATAL_FAILURE(func());
            FAIL() << "expected ErpException";
        }
        catch (const ErpException& erpException)
        {
            EXPECT_EQ(erpException.what(), message);
            if (GetParam() == Configuration::GenericValidationMode::disable)
            {
                if (shortDiagnostics.empty())
                {
                    EXPECT_FALSE(erpException.diagnostics().has_value());
                }
                else
                {
                    ASSERT_TRUE(erpException.diagnostics().has_value());
                    EXPECT_EQ(erpException.diagnostics().value(), shortDiagnostics);
                }
            }
            else
            {
                ASSERT_TRUE(erpException.diagnostics().has_value());
                std::vector<std::string> diagnosticsMessages = String::split(erpException.diagnostics().value(), "; ");
                std::vector<std::string> fullDiagnosticsMessages = String::split(fullDiagnostics, "; ");
                std::set<std::string> diagnosticsMessagesSet(diagnosticsMessages.begin(), diagnosticsMessages.end());
                std::set<std::string> fullDiagnosticsMessagesSet(fullDiagnosticsMessages.begin(),
                                                                 fullDiagnosticsMessages.end());
                EXPECT_EQ(diagnosticsMessagesSet, fullDiagnosticsMessagesSet);
            }
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "expected ErpException but got: " << typeid(ex).name() << ": " << ex.what();
        }
    }
        // clang-format off
    const std::string kbvMessage = oldProfilesAllowed()
        ?"missing valueCoding.code in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis"
        :"FHIR-Validation error";
    const std::string kbvFullDiagnosticsOld =
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding.code: "
        "error: missing mandatory element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition:rechtsgrundlage:valueCoding|1.0.2); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding: "
        "error: Expected exactly one system and one code sub-element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle.entry[0].resource{Composition}.extension[0].valueCoding.code: "
        "error: missing mandatory element "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis:valueCoding|1.0.3); "
    "Bundle.entry[0].resource{Composition}.subject: "
        "error: Cannot match profile to Element 'Composition': "
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.0.3 "
            "(referenced resource Bundle.entry[0].resource{Composition} must match one of: "
                "[\"https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.0.3\"]); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1); ";
    const std::string kbvFullDiagnosticsNew =
    "Bundle.entry: error: At most 1 elements expected, but got 2 "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle:Einrichtung|1.1.0); "
    "Bundle.entry[*]:Einrichtung: error: At most 1 elements expected, but got 2 "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle:Einrichtung|1.1.0); "
    "Bundle: error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1); "
    "Bundle: error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); ";
    // clang-format on
    const std::string& kbvFullDiagnostics = oldProfilesAllowed()?kbvFullDiagnosticsOld:kbvFullDiagnosticsNew;


    ResourceManager& resourceManager = ResourceManager::instance();
    const std::string dataPath = "test/EndpointHandlerTest";
};

TEST_P(ResourceGenericValidationTest, genericValidationModeXMLnoSchema)
{
    // clang-format off
    static const std::string message = "XML error on line 0";
    static const std::string shortDiagnostics =
        "Element '{http://hl7.org/fhir}extension': The attribute 'url' is required but missing.";
    static const std::string fullDiagnostics =
        "Element '{http://hl7.org/fhir}extension': The attribute 'url' is required but missing.";
    // clang-format on

    EnvironmentVariableGuard validationModeGuardOld{"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromXml(goodParametersXML()));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromXml(genericFailParametersXML()), ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromXml(genericFailParametersXML()));
    }
    EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
        [&] {
            fromXml(badParametersXML());
        },
        message, shortDiagnostics, fullDiagnostics));
}

TEST_P(ResourceGenericValidationTest, genericValidationModeJSONnoSchema)
{
    // clang-format off
    const std::string message = (oldProfilesAllowed())?"XML error on line 0":"FHIR-Validation error";

    static const std::string shortDiagnostics =
        "Element '{http://hl7.org/fhir}valueCode': This element is not expected."
            " Expected is ( {http://hl7.org/fhir}resource ).";

    std::string fullDiagnostics =
        "Parameters.parameter[0].extension[0].url: "
            "error: missing mandatory element (from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); ";
    if (oldProfilesAllowed())
    {
        fullDiagnostics += "error: Element '{http://hl7.org/fhir}valueCode': "
            "This element is not expected. Expected is ( {http://hl7.org/fhir}resource ).; ";
    }
    // clang-format on
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    EnvironmentVariableGuard validationModeGuardOld{"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE",
                                                    std::string{magic_enum::enum_name(GetParam())}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromJson(goodParametersJSON()));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromJson(genericFailParametersJSON()), ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromJson(genericFailParametersJSON()));
    }
    auto validateBad = [&] { fromJson(badParametersJSON(), SchemaType::ActivateTaskParameters); };
    if (oldProfilesAllowed() || GetParam() != Configuration::GenericValidationMode::disable)
    {
        EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(validateBad, message, shortDiagnostics, fullDiagnostics));
    }
}


TEST_P(ResourceGenericValidationTest, genericValidationModeXMLKbvBundle)
{
    using namespace std::string_literals;
    // clang-format off

    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    EnvironmentVariableGuard validationModeGuardOld{"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true,
                                        .levels{.unreferencedBundledResource = fhirtools::Severity::warning,
                                                .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromXml<model::KbvBundle>(goodBundleXML(), SchemaType::KBV_PR_ERP_Bundle, options));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromXml<model::KbvBundle>(genericFailBundleXML(), SchemaType::KBV_PR_ERP_Bundle, options),
                                               ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromXml<model::KbvBundle>(genericFailBundleXML(), SchemaType::KBV_PR_ERP_Bundle, options));
    }
    if (oldProfilesAllowed() || GetParam() != Configuration::GenericValidationMode::disable)
    {
        EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
            [&] {
                fromXml<model::KbvBundle>(badBundleXML(), SchemaType::KBV_PR_ERP_Bundle, options);
            },
            kbvMessage, {}, kbvFullDiagnostics));
    }
}

TEST_P(ResourceGenericValidationTest, genericValidationModeJsonKbvBundle)
{
    using namespace std::string_literals;

    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    EnvironmentVariableGuard validationModeGuardOld{"ERP_SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true,
                                        .levels{.unreferencedBundledResource = fhirtools::Severity::warning,
                                                .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromJson<model::KbvBundle>(goodBundleJSON(), SchemaType::KBV_PR_ERP_Bundle, options));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromJson<model::KbvBundle>(genericFailBundleJSON(), SchemaType::KBV_PR_ERP_Bundle, options),
                     ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromJson<model::KbvBundle>(genericFailBundleJSON(), SchemaType::KBV_PR_ERP_Bundle, options));
    }
    if (oldProfilesAllowed() || GetParam() != Configuration::GenericValidationMode::disable)
    {
        EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
            [&] {
                fromJson<model::KbvBundle>(badBundleJSON(), SchemaType::KBV_PR_ERP_Bundle, options);
            },
            kbvMessage, {}, kbvFullDiagnostics));
    }
}


INSTANTIATE_TEST_SUITE_P(AllModes, ResourceGenericValidationTest,
                         ::testing::ValuesIn(magic_enum::enum_values<Configuration::GenericValidationMode>()),
                         [](auto info) {
                             return std::string{magic_enum::enum_name(info.param)};
                         });

TEST(ResourceFactoryTest, invalid_profile)
{
    using enum model::ResourceVersion::FhirProfileBundleVersion;
    auto mdisp = ResourceTemplates::medicationDispenseXml({});
    mdisp = String::replaceAll(mdisp, "https://gematik.de/", "https://invalid/");
    using Factory = model::ResourceFactory<model::MedicationDispense>;
    try {
        Factory::fromXml(mdisp, *StaticData::getXmlValidator()).getValidated(SchemaType::Gem_erxMedicationDispense, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator());
        GTEST_FAIL() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        const auto& supported = model::ResourceVersion::supportedBundles();
        if (supported == std::set{v_2022_01_01, v_2023_07_01})
        {
            EXPECT_EQ(*ex.diagnostics(), R"(unsupported or unknown profile version expected one of: [)"
                    R"("https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense|1.1.1", )"
                    R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2"])");
        }
        else if (supported == std::set{v_2022_01_01})
        {
            EXPECT_EQ(*ex.diagnostics(), R"(unsupported or unknown profile version expected one of: [)"
                    R"("https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense|1.1.1"])");
        }
        else
        {
            EXPECT_EQ(*ex.diagnostics(), R"(unsupported or unknown profile version expected one of: [)"
                    R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2"])");
        }

    }
    catch (const std::exception& ex)
    {
        GTEST_FAIL() << "Unexpected Exception " << typeid(ex).name() << ": " << ex.what();
    }
}
