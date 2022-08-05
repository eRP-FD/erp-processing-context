/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/String.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"

#include <erp/util/Configuration.hxx>
#include <gtest/gtest.h>
#include <test/util/StaticData.hxx>
#include <array>
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
    const auto [gematikVersion, kbvVersion] = ResourceVersion::current();

    const ::std::array<::std::pair<::std::string_view, ::std::string_view>, 3> profileTypes = {
        ::std::make_pair("https://gematik.de/fhir/StructureDefinition/ErxReceipt"sv,
                         ResourceVersion::v_str(gematikVersion)),
        ::std::make_pair("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle"sv,
                         ResourceVersion::v_str(kbvVersion)),
        ::std::make_pair("http://hl7.org/fhir/StructureDefinition/Binary"sv, "4.0.1"sv)};


    ::std::unique_ptr<FriendlyResourceBase> resource;
    for (const auto& type : profileTypes)
    {
        ASSERT_NO_THROW(resource.reset(new FriendlyResourceBase{type.first}));
        const auto profile = resource->getOptionalStringValue(::rapidjson::Pointer{"/meta/profile/0"});
        ASSERT_TRUE(profile);
        if (ResourceVersion::current<ResourceVersion::DeGematikErezeptWorkflowR4>() ==
            ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1)
        {
            const auto profileParts = String::split(profile.value(), '|');
            ASSERT_EQ(profileParts.size(), 1);
            EXPECT_EQ(profileParts[0], type.first);
        }
        else
        {
            const auto profileParts = String::split(profile.value(), '|');
            ASSERT_EQ(profileParts.size(), 2);
            EXPECT_EQ(profileParts[0], type.first);
            EXPECT_EQ(profileParts[1], type.second);
        }
    }

    {
        EXPECT_THROW(resource.reset(new FriendlyResourceBase{""}), ::std::logic_error);
        EXPECT_THROW(resource.reset(new FriendlyResourceBase{"https://company.com/invalid/profile"}),
                     ::std::logic_error);
    }
}
}

class ResourceGenericValidationTest : public ::testing::TestWithParam<Configuration::GenericValidationMode>
{
protected:
    template<typename ResourceT = model::Parameters>
    ResourceT fromXml(std::string_view xml, SchemaType schemaType = SchemaType::fhir)
    {
        return ResourceT::fromXml(xml, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), schemaType);
    }
    template<typename ResourceT = model::Parameters>
    ResourceT fromJson(std::string_view json, SchemaType schemaType = SchemaType::fhir)
    {
        return ResourceT::fromJson(json, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                                   *StaticData::getInCodeValidator(), schemaType);
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
            "test/validation/xml/parameters/simpleParameters_invalid_boolean.json");
    }

    std::string goodBundleXML()
    {
        return resourceManager.getStringResource(dataPath + "/kbv_bundle.xml");
    }
    std::string genericFailBundleXML()
    {
        return resourceManager.getStringResource(dataPath + "/kbv_bundle_duplicate_fullUrl.xml");
    }
    std::string badBundleXML()
    {
        return resourceManager.getStringResource(dataPath + "/kbv_bundle_duplicate_fullUrl_missing_code.xml");
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
            ADD_FAILURE() << "expected ErpException";
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
                EXPECT_EQ(erpException.diagnostics().value(), fullDiagnostics);
            }
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "expected ErpException but got: " << typeid(ex).name() << ": " << ex.what();
        }
    }

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
        "error: Element '{http://hl7.org/fhir}extension': The attribute 'url' is required but missing.; "
        "Parameters.parameter[0].extension[0].url: "
        "error: missing mandatory element "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); ";
    // clang-format on

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
    static const std::string message = "validation of JSON document failed";
    static const std::string fullDiagnostics =
        "Parameters.parameter[0].extension[0].url: "
            "error: missing mandatory element "
            "(from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); ";
    // clang-format on
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
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
    EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
        [&] {
            fromJson(badParametersJSON());
        },
        message, {}, fullDiagnostics));
}


TEST_P(ResourceGenericValidationTest, genericValidationModeXMLKbvBundle)
{
    using namespace std::string_literals;
    // clang-format off
    const auto message = "missing valueCoding.code in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis"s;
    const auto fullDiagnostics =
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
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2); "s;
    // clang-format on


    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromXml<model::KbvBundle>(goodBundleXML(), SchemaType::KBV_PR_ERP_Bundle));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromXml<model::KbvBundle>(genericFailBundleXML(), SchemaType::KBV_PR_ERP_Bundle), ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromXml<model::KbvBundle>(genericFailBundleXML(), SchemaType::KBV_PR_ERP_Bundle));
    }
    EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
        [&] {
            fromXml<model::KbvBundle>(badBundleXML(), SchemaType::KBV_PR_ERP_Bundle);
        },
        message, {}, fullDiagnostics));
}

TEST_P(ResourceGenericValidationTest, genericValidationModeJsonKbvBundle)
{
    using namespace std::string_literals;
    // clang-format off
    const auto message = "missing valueCoding.code in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis"s;
    const auto fullDiagnostics =
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
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1); "
    "Bundle: "
        "error: bdl-7: FullUrl must be unique in a bundle, or else entries with the same fullUrl must have different meta.versionId (except in history bundles) "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.2); "s;
    // clang-format on


    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                 std::string{magic_enum::enum_name(GetParam())}};
    std::optional<model::Parameters> params;
    EXPECT_NO_THROW(fromJson<model::KbvBundle>(goodBundleJSON(), SchemaType::KBV_PR_ERP_Bundle));
    if (GetParam() == Configuration::GenericValidationMode::require_success)
    {
        EXPECT_THROW(fromJson<model::KbvBundle>(genericFailBundleJSON(), SchemaType::KBV_PR_ERP_Bundle), ErpException);
    }
    else
    {
        EXPECT_NO_THROW(fromJson<model::KbvBundle>(genericFailBundleJSON(), SchemaType::KBV_PR_ERP_Bundle));
    }
    EXPECT_NO_FATAL_FAILURE(withDiagnosticCheck(
        [&] {
            fromJson<model::KbvBundle>(badBundleJSON(), SchemaType::KBV_PR_ERP_Bundle);
        },
        message, {}, fullDiagnostics));
}


INSTANTIATE_TEST_SUITE_P(AllModes, ResourceGenericValidationTest,
                         ::testing::ValuesIn(magic_enum::enum_values<Configuration::GenericValidationMode>()),
                         [](auto info) {
                             return std::string{magic_enum::enum_name(info.param)};
                         });
