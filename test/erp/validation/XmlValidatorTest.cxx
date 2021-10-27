/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/XmlValidator.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <magic_enum.hpp>
#include <ostream>

#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"
#include "test_config.h"

namespace fs = std::filesystem;

class XmlValidatorTest : public testing::Test
{
public:
    std::shared_ptr<XmlValidator> getXmlValidator()
    {
        return StaticData::getXmlValidator();
    }

    static model::Task validTask()
    {
        model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1");
        task.setPrescriptionId(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
        return task;
    }
};


TEST_F(XmlValidatorTest, ValidateDefaultTask)
{
    auto xmlDocument = validTask().serializeToXmlString();
    ASSERT_NO_THROW((void)FhirConverter().xmlStringToJsonWithValidation(
        xmlDocument, *getXmlValidator(), SchemaType::Gem_erxTask));
}

TEST_F(XmlValidatorTest, TaskInvalidPrescriptionId)
{
    GTEST_SKIP_("Prescription ID pattern not checked by xsd");
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer prescriptionIdPointer("/identifier/0/value");
    prescriptionIdPointer.Set(jsonDocument, "invalid_id");
    task = model::Task::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(jsonDocument));

    // now the validation must fail.
    ASSERT_ANY_THROW((void)FhirConverter().xmlStringToJsonWithValidation(
        task.serializeToXmlString(), *getXmlValidator(), SchemaType::Gem_erxTask));
}


TEST_F(XmlValidatorTest, getSchemaValidationContext)
{
    for(const auto& type : magic_enum::enum_values<SchemaType>())
    {
        ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(type) != nullptr);
    }
}

TEST_F(XmlValidatorTest, MinimalFhirDocumentMustFail)
{
    const auto* const document = R"(
<Communication xmlns="http://hl7.org/fhir">
    <meta>
        <profile value="https://gematik.de/fhir/StructureDefinition/erxCommunicationDispReq"/>
    </meta>
</Communication>
)";
    ASSERT_THROW(FhirConverter().xmlStringToJsonWithValidation(
                     document, *getXmlValidator(), SchemaType::Gem_erxCommunicationDispReq),
                 ErpException);
}


struct TestParams
{
    friend std::ostream& operator<<(std::ostream& os, const TestParams& params);
    bool valid;
    fs::path basePath;
    std::string startsWith;
    SchemaType schemaType;
};
std::ostream& operator<<(std::ostream& os, const TestParams& params)
{
    os << "should be " << (params.valid?"valid: ":"invalid: ") << magic_enum::enum_name(params.schemaType)
       << " " << params.startsWith;
    return os;
}


class XmlValidatorTestParams : public testing::TestWithParam<TestParams>
{
public:
    std::shared_ptr<XmlValidator> getXmlValidator()
    {
        return StaticData::getXmlValidator();
    }
};
TEST_P(XmlValidatorTestParams, Resources)
{
    for (const auto& dirEntry : fs::directory_iterator(GetParam().basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), GetParam().startsWith))
        {
            const auto document = FileHelper::readFileAsString(dirEntry.path());
            if (GetParam().valid)
            {
                EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
                    document, *getXmlValidator(), GetParam().schemaType))
                                << "Test failed for file: " << dirEntry.path().filename().string();
            }
            else
            {
                EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(
                    document, *getXmlValidator(), GetParam().schemaType), ErpException)
                                << "Test failed for file: " << dirEntry.path().filename().string();
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ValidResources, XmlValidatorTestParams,
    testing::Values(
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/", "AuditEvent_valid", SchemaType::Gem_erxAuditEvent},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_valid", SchemaType::Gem_erxBundle},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/", "CommunicationDispReq_valid", SchemaType::Gem_erxCommunicationDispReq},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/", "CommunicationInfoReq_valid", SchemaType::Gem_erxCommunicationInfoReq},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/", "CommunicationReply_valid", SchemaType::Gem_erxCommunicationReply},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/", "CommunicationRepresentative_valid", SchemaType::Gem_erxCommunicationRepresentative},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/composition/", "Composition_valid", SchemaType::Gem_erxCompositionElement},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_valid", SchemaType::Gem_erxDevice},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/", "Bundle_valid", SchemaType::KBV_PR_ERP_Bundle},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/simplifierSamples/", "", SchemaType::KBV_PR_ERP_Bundle},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/composition/", "Composition_valid", SchemaType::KBV_PR_ERP_Composition},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/coverage/", "Coverage_valid", SchemaType::KBV_PR_FOR_Coverage},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationcompounding/", "Medication_valid", SchemaType::KBV_PR_ERP_Medication_Compounding},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationfreetext/", "Medication_valid", SchemaType::KBV_PR_ERP_Medication_FreeText},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationingredient/", "Medication_valid", SchemaType::KBV_PR_ERP_Medication_Ingredient},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationpzn/", "Medication_valid", SchemaType::KBV_PR_ERP_Medication_PZN},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/organization/", "Organization_valid", SchemaType::KBV_PR_FOR_Organization},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/patient/", "Patient_valid", SchemaType::KBV_PR_FOR_Patient},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practicesupply/", "PracticeSupply_valid", SchemaType::KBV_PR_ERP_PracticeSupply},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitioner/", "Practitioner_valid", SchemaType::KBV_PR_FOR_Practitioner},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitionerrole/", "PractitionerRole_valid", SchemaType::KBV_PR_FOR_PractitionerRole},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/prescription/", "Prescription_valid", SchemaType::KBV_PR_ERP_Prescription},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/", "MedicationDispense_valid", SchemaType::Gem_erxMedicationDispense},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/organization/", "Organization_valid", SchemaType::Gem_erxOrganizationElement},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "activateTaskParameters_valid", SchemaType::ActivateTaskParameters},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "createTaskParameters_valid", SchemaType::CreateTaskParameters},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/", "ReceiptBundle_valid", SchemaType::Gem_erxReceiptBundle},
        TestParams{true, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_valid", SchemaType::Gem_erxTask},

        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/", "AuditEvent_invalid", SchemaType::Gem_erxAuditEvent},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_invalid", SchemaType::Gem_erxBundle},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/", "CommunicationDispReq_invalid", SchemaType::Gem_erxCommunicationDispReq},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/", "CommunicationInfoReq_invalid", SchemaType::Gem_erxCommunicationInfoReq},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/", "CommunicationReply_invalid", SchemaType::Gem_erxCommunicationReply},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/", "CommunicationRepresentative_invalid", SchemaType::Gem_erxCommunicationRepresentative},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/composition/", "Composition_invalid", SchemaType::Gem_erxCommunicationReply},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_invalid", SchemaType::Gem_erxDevice},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/", "MedicationDispense_invalid", SchemaType::Gem_erxMedicationDispense},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/organization/", "Organization_invalid", SchemaType::Gem_erxOrganizationElement},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "activateTaskParameters_invalid", SchemaType::ActivateTaskParameters},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "createTaskParameters_invalid", SchemaType::CreateTaskParameters},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/", "ReceiptBundle_invalid", SchemaType::Gem_erxReceiptBundle},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_invalid", SchemaType::Gem_erxTask},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/bundle/", "Bundle_invalid", SchemaType::KBV_PR_ERP_Bundle},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/composition/", "Composition_invalid", SchemaType::KBV_PR_ERP_Composition},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/coverage/", "Coverage_invalid", SchemaType::KBV_PR_FOR_Coverage},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationcompounding/", "Medication_invalid", SchemaType::KBV_PR_ERP_Medication_Compounding},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationfreetext/", "Medication_invalid", SchemaType::KBV_PR_ERP_Medication_FreeText},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationingredient/", "Medication_invalid", SchemaType::KBV_PR_ERP_Medication_Ingredient},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/medicationpzn/", "Medication_invalid", SchemaType::KBV_PR_ERP_Medication_PZN},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/organization/", "Organization_invalid", SchemaType::KBV_PR_FOR_Organization},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/patient/", "Patient_invalid", SchemaType::KBV_PR_FOR_Patient},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practicesupply/", "PracticeSupply_invalid", SchemaType::KBV_PR_ERP_PracticeSupply},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitioner/", "Practitioner_invalid", SchemaType::KBV_PR_FOR_Practitioner},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/practitionerrole/", "PractitionerRole_invalid", SchemaType::KBV_PR_FOR_PractitionerRole},
        TestParams{false, fs::path(TEST_DATA_DIR) / "validation/xml/kbv/prescription/", "Prescription_invalid", SchemaType::KBV_PR_FOR_PractitionerRole}));

TEST_F(XmlValidatorTest, Erp6345)
{
    auto file1 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile.xml");
    EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(file1, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle), ErpException);

    auto file2 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile_valid.xml");
    EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(file2, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle));
}
