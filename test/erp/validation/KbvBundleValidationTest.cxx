/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/KbvBundle.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <magic_enum.hpp>
#include <ostream>

#include "test_config.h"

namespace fs = std::filesystem;

struct TestParamsKbvBundle {
    bool valid;
    fs::path path;
    SchemaType schemaType;
    model::ResourceVersion::KbvItaErp version;
};
std::ostream& operator<<(std::ostream& os, const TestParamsKbvBundle& params)
{
    os << params.path.filename();
    return os;
}

static std::vector<TestParamsKbvBundle> makeKbvParams(bool valid, const fs::path& basePath, std::string_view startsWith,
                                                      SchemaType schemaType, model::ResourceVersion::KbvItaErp version)
{
    std::vector<TestParamsKbvBundle> params;
    for (const auto& dirEntry : fs::directory_iterator(basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), startsWith))
        {
            params.push_back({valid, dirEntry.path(), schemaType, version});
        }
    }
    return params;
}

class XmlValidatorTestParamsKbvBundle : public testing::TestWithParam<TestParamsKbvBundle>
{
};

TEST_P(XmlValidatorTestParamsKbvBundle, Resources)//NOLINT(readability-function-cognitive-complexity)
{
    ASSERT_EQ(GetParam().schemaType, SchemaType::KBV_PR_ERP_Bundle) << "Test called with wrong schema type";
    const auto document = FileHelper::readFileAsString(GetParam().path);
    if (GetParam().valid)
    {
        EXPECT_NO_THROW(model::KbvBundle::fromXml(document, *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle));
    }
    else
    {
        EXPECT_THROW(model::KbvBundle::fromXml(document, *StaticData::getXmlValidator(),
                                               *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle),
                     ErpException);
    }
}

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_BundleValid, XmlValidatorTestParamsKbvBundle,
                         testing::ValuesIn(makeKbvParams(true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/",
                                                         "Bundle_valid", SchemaType::KBV_PR_ERP_Bundle,
                                                         model::ResourceVersion::KbvItaErp::v1_0_2)));
 INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_BundleSimplifierSamples, XmlValidatorTestParamsKbvBundle,
                          testing::ValuesIn(makeKbvParams(
                              true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/simplifierSamples/", "",
                              SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2)));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_BundleInvalid, XmlValidatorTestParamsKbvBundle,
                         testing::ValuesIn(makeKbvParams(false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/",
                                                         "Bundle_invalid", SchemaType::KBV_PR_ERP_Bundle,
                                                         model::ResourceVersion::KbvItaErp::v1_0_2)));


// -----------------------------

struct TestParamsKbvWithEmbedded {
    bool valid;
    fs::path path;
    SchemaType schemaType;
    model::ResourceVersion::KbvItaErp version;
    fs::path placeholderPath;
};
std::ostream& operator<<(std::ostream& os, const TestParamsKbvWithEmbedded& params)
{
    os << params.path.filename();
    return os;
}
class KbvWithEmbeddedValidationTestParams : public testing::TestWithParam<TestParamsKbvWithEmbedded>
{
};

static std::vector<TestParamsKbvWithEmbedded>
makeKbvWithPlaceholderParams(bool valid, const fs::path& basePath, std::string_view startsWith, SchemaType schemaType,
                             model::ResourceVersion::KbvItaErp version, const fs::path& placeholderFile)
{
    std::vector<TestParamsKbvWithEmbedded> params;
    for (const auto& dirEntry : fs::directory_iterator(basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), startsWith))
        {
            params.push_back({valid, dirEntry.path(), schemaType, version, placeholderFile});
        }
    }
    return params;
}

TEST_P(KbvWithEmbeddedValidationTestParams, KbvBundleWithEmbeddedSubResource)//NOLINT(readability-function-cognitive-complexity)
{
    // Load the standalone Medication test files and embed them into a KbvBundle. Validate this KbvBundle

    ASSERT_EQ(GetParam().schemaType, SchemaType::KBV_PR_ERP_Bundle) << "Test called with wrong schema type";

    auto kbvBundle = FileHelper::readFileAsString(GetParam().placeholderPath);
    auto embeddedResource = FileHelper::readFileAsString(GetParam().path);
    embeddedResource = String::replaceAll(embeddedResource, " xmlns=\"http://hl7.org/fhir\"", "");

    kbvBundle = String::replaceAll(kbvBundle, "####INSERT RESOURCE HERE####", embeddedResource);

    if (GetParam().valid)
    {
        EXPECT_NO_THROW(model::KbvBundle::fromXml(kbvBundle, *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle));
    }
    else
    {
        EXPECT_THROW(model::KbvBundle::fromXml(kbvBundle, *StaticData::getXmlValidator(),
                                               *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle),
                     ErpException);
    }
}
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationCompounding_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationcompounding/",
                             "Medication_valid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationCompounding_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationcompounding/",
                             "Medication_invalid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationFreeText_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationfreetext/",
                             "Medication_valid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationFreeText_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationfreetext/",
                             "Medication_invalid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationIngredient_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationingredient/",
                             "Medication_valid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationIngredient_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationingredient/",
                             "Medication_invalid", SchemaType::KBV_PR_ERP_Bundle,
                             model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationPZN_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationpzn/", "Medication_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_MedicationPZN_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationpzn/", "Medication_invalid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Medication.xml")));

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Composition_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/composition/", "Composition_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) /
                                 "validation/xml/kbv/bundle/Bundle_placeholder_Composition.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Composition_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/composition/", "Composition_invalid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) /
                                 "validation/xml/kbv/bundle/Bundle_placeholder_Composition.xml")));

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Coverage_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/coverage/", "Coverage_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Coverage.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Coverage_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/coverage/", "Coverage_invalid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Coverage.xml")));

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Organization_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/organization/", "Organization_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) /
                                 "validation/xml/kbv/bundle/Bundle_placeholder_Organization.xml")));
INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_Organization_Invalid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/organization/", "Organization_invalid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Organization.xml")));

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Patient_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/patient/", "Patient_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Patient.xml")));
INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Patient_Invalid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/patient/", "Patient_invalid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Patient.xml")));

INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_PracticeSupply_Valid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practicesupply/", "PracticeSupply_valid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_PracticeSupply.xml")));
INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_PracticeSupply_Invalid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practicesupply/", "PracticeSupply_invalid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_PracticeSupply.xml")));

INSTANTIATE_TEST_SUITE_P(KBV_PR_ERP_Bundle_embedded_Practitioner_Valid, KbvWithEmbeddedValidationTestParams,
                         testing::ValuesIn(makeKbvWithPlaceholderParams(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitioner/", "Practitioner_valid",
                             SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
                             fs::path(TEST_DATA_DIR) /
                                 "validation/xml/kbv/bundle/Bundle_placeholder_Practitioner.xml")));
INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_Practitioner_Invalid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitioner/", "Practitioner_invalid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_Practitioner.xml")));

INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_PractitionerRole_Valid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitionerrole/", "PractitionerRole_valid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_PractitionerRole.xml")));
INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_PractitionerRole_Invalid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitionerrole/", "PractitionerRole_invalid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_PractitionerRole.xml")));

INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_MedicationRequest_Valid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/prescription/", "Prescription_valid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_MedicationRequest.xml")));
INSTANTIATE_TEST_SUITE_P(
    KBV_PR_ERP_Bundle_embedded_MedicationRequest_Invalid, KbvWithEmbeddedValidationTestParams,
    testing::ValuesIn(makeKbvWithPlaceholderParams(
        false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/prescription/", "Prescription_invalid",
        SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2,
        fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/Bundle_placeholder_MedicationRequest.xml")));
