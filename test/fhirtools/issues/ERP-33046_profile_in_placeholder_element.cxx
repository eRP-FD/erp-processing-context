/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <ranges>

class Erp33046ProfileInPlaceholderElement : public testing::Test
{
public:
    static constexpr char extensionUrl[] =
        "http://fhir-tools.test/minifhirtypes/extension/ERP-33046_profile_in_placeholder_element";
    static constexpr char resourceUrl[] =
        "http://fhir-tools.test/minifhirtypes/resource/ERP-33046_profile_in_placeholder_element";
    void SetUp() override
    {
        repo.load(
            {
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
                ResourceManager::getAbsoluteFilename(
                    "test/fhir-path/profiles/ERP-33046_profile_in_placeholder_element.xml"),
            },
            resolver);

        view = fhirtools::FhirResourceViewGroupSet::create("testView", resolver.group(), &repo);
    }

protected:
    fhirtools::FhirStructureRepositoryBackend repo;
    fhirtools::FhirResourceGroupConst resolver{"test"};
    std::shared_ptr<fhirtools::FhirResourceViewGroupSet> view;
    // fhirtools::ValidationResults validate(std::string_view sample, const fhirtools::ValidatorOptions& options)
    // {
    //     auto doc = model::NumberAsStringParserDocument::fromJson(sample);
    //     auto element = std::make_shared<ErpElement>(&repo, std::weak_ptr<ErpElement>{}, "Resource", &doc);
    //     return fhirtools::FhirPathValidator::validateWithProfiles(
    //         view, element, "Resource", std::set{fhirtools::DefinitionKey{GetParam()}}, options);
    // }
};

TEST_F(Erp33046ProfileInPlaceholderElement, PlaceholerElementTypeMembers)
{
    using namespace fhirtools::version_literal;
    const auto* extensionDef = repo.findDefinition(extensionUrl, "4.0.1"_ver);
    ASSERT_NE(extensionDef, nullptr);
    {
        const auto element = extensionDef->findElement("Extension.valueQuantity");
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(element->typeId(), "Quantity");
        const auto& profiles = element->profiles();
        EXPECT_EQ(profiles.size(), 1);
        EXPECT_TRUE(profiles.contains("http://hl7.org/fhir/StructureDefinition/NoSystemQuantity"));
        EXPECT_TRUE(element->referenceTargetProfiles().empty());
    }
    {
        const auto element = extensionDef->findElement("Extension.valueCoding");
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(element->typeId(), "Coding");
        const auto& profiles = element->profiles();
        EXPECT_EQ(profiles.size(), 2);
        EXPECT_TRUE(profiles.contains("http://hl7.org/fhir/StructureDefinition/coding/Dummy1"));
        EXPECT_TRUE(profiles.contains("http://hl7.org/fhir/StructureDefinition/coding/Dummy2"));
        EXPECT_TRUE(element->referenceTargetProfiles().empty());
    }
    {
        const auto element = extensionDef->findElement("Extension.valueReference");
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(element->typeId(), "Reference");
        const auto& profiles = element->profiles();
        EXPECT_EQ(profiles.size(), 1);
        EXPECT_TRUE(profiles.contains("http://fhir-tools.test/minifhirtypes/reference/Dummy"));
        const auto& targetProfiles = element->referenceTargetProfiles();
        EXPECT_EQ(targetProfiles.size(), 2);
        EXPECT_TRUE(targetProfiles.contains("SomeTargetProfile1"));
        EXPECT_TRUE(targetProfiles.contains("SomeTargetProfile2"));
    }
}


TEST_F(Erp33046ProfileInPlaceholderElement, NormalElementTypeMembers)
{
    using namespace fhirtools::version_literal;
    const auto* extensionDef = repo.findDefinition(resourceUrl, "4.0.1"_ver);
    ASSERT_NE(extensionDef, nullptr);
    {
        const auto element = extensionDef->findElement("ResourceErp33046.reference");
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(element->typeId(), "Reference");
        const auto& profiles = element->profiles();
        EXPECT_EQ(profiles.size(), 1);
        EXPECT_TRUE(profiles.contains("http://fhir-tools.test/minifhirtypes/reference/Dummy"));
        const auto& targetProfiles = element->referenceTargetProfiles();
        EXPECT_EQ(targetProfiles.size(), 2);
        EXPECT_TRUE(targetProfiles.contains("SomeOtherTargetProfile1"));
        EXPECT_TRUE(targetProfiles.contains("SomeOtherTargetProfile2"));
    }
}


TEST_F(Erp33046ProfileInPlaceholderElement, ProfiledPlaceholderElementValidation_success)
{
    static constexpr char validResourceErp33046[] = R"(
        {
            "resourceType": "ResourceErp33046",
            "id": "validResourceErp33046",
            "extension": [
                {
                    "url": "http://fhir-tools.test/minifhirtypes/extension/ERP-33046_profile_in_placeholder_element",
                    "valueQuantity": {
                        "unit": "pc",
                        "value": 8.178,
                        "code": "SrgAStar"
                    }
                }
            ]
        }
    )";
    auto doc = model::NumberAsStringParserDocument::fromJson(validResourceErp33046);
    auto element = std::make_shared<ErpElement>(&repo, std::weak_ptr<ErpElement>{}, "ResourceErp33046", &doc);
    auto result = fhirtools::FhirPathValidator::validate(view, element, "ResourceErp33046");
    if (result.highestSeverity() > fhirtools::Severity::warning)
    {
        result.dumpToLog();
        ADD_FAILURE();
    }
}


TEST_F(Erp33046ProfileInPlaceholderElement, ProfiledPlaceholderElementValidation_failure)
{
    static constexpr char invalidResourceErp33046[] = R"(
        {
            "resourceType": "ResourceErp33046",
            "id": "validResourceErp33046",
            "extension": [
                {
                    "url": "http://fhir-tools.test/minifhirtypes/extension/ERP-33046_profile_in_placeholder_element",
                    "valueQuantity": {
                        "unit": "pc",
                        "value": 8.178,
                        "system": "sol",
                        "code": "SrgAStar"
                    }
                }
            ]
        }
    )";
    auto doc = model::NumberAsStringParserDocument::fromJson(invalidResourceErp33046);
    auto element = std::make_shared<ErpElement>(&repo, std::weak_ptr<ErpElement>{}, "ResourceErp33046", &doc);
    auto validationResult = fhirtools::FhirPathValidator::validate(view, element, "ResourceErp33046");
    const auto& resultSet = validationResult.results();
    auto err = std::ranges::find(resultSet, fhirtools::Severity::error, &fhirtools::ValidationError::severity);
    ASSERT_NE(err, resultSet.end()) << (validationResult.dumpToLog(), "");
    ASSERT_NE(err->profile, nullptr) << (validationResult.dumpToLog(), "");
    const auto& profileKey = err->profile->key();
    EXPECT_EQ(profileKey.url, "http://hl7.org/fhir/StructureDefinition/NoSystemQuantity")
        << (validationResult.dumpToLog(), "");
    EXPECT_EQ(err->text(), "At most 0 elements expected, but got 1") << (validationResult.dumpToLog(), "");
    EXPECT_EQ(err->fieldName, "ResourceErp33046.extension[0].valueQuantity.system")
        << (validationResult.dumpToLog(), "");
}
