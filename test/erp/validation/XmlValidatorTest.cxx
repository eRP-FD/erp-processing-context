/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/XmlValidator.hxx"
#include "test/util/StaticData.hxx"

#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <magic_enum.hpp>
#include <ostream>

#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/KbvBundle.hxx"
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
    ASSERT_NO_THROW((void) FhirConverter().xmlStringToJsonWithValidation(
        xmlDocument, *getXmlValidator(), SchemaType::Gem_erxTask,
        ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>()));
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
        task.serializeToXmlString(), *getXmlValidator(), SchemaType::Gem_erxTask,
        model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>()));
}


TEST_F(XmlValidatorTest, getSchemaValidationContext)
{
    for(const auto& type : magic_enum::enum_values<SchemaType>())
    {
        switch(type)
        {
            case SchemaType::fhir:
            case SchemaType::BNA_tsl:
            case SchemaType::Gematik_tsl:
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContextNoVer(type) != nullptr);
                break;
            case SchemaType::Gem_erxAuditEvent:
            case SchemaType::Gem_erxBinary:
            case SchemaType::Gem_erxCommunicationDispReq:
            case SchemaType::Gem_erxCommunicationInfoReq:
            case SchemaType::Gem_erxCommunicationReply:
            case SchemaType::Gem_erxCommunicationRepresentative:
            case SchemaType::Gem_erxCompositionElement:
            case SchemaType::Gem_erxDevice:
            case SchemaType::Gem_erxMedicationDispense:
            case SchemaType::Gem_erxOrganizationElement:
            case SchemaType::Gem_erxReceiptBundle:
            case SchemaType::Gem_erxTask:
                ASSERT_TRUE(
                    getXmlValidator()->getSchemaValidationContext(
                        type,
                        ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>()) !=
                    nullptr);
                break;
            case SchemaType::KBV_PR_ERP_Bundle:
            case SchemaType::KBV_PR_ERP_Composition:
            case SchemaType::KBV_PR_ERP_Medication_Compounding:
            case SchemaType::KBV_PR_ERP_Medication_FreeText:
            case SchemaType::KBV_PR_ERP_Medication_Ingredient:
            case SchemaType::KBV_PR_ERP_Medication_PZN:
            case SchemaType::KBV_PR_ERP_PracticeSupply:
            case SchemaType::KBV_PR_ERP_Prescription:
            case SchemaType::KBV_PR_FOR_Coverage:
            case SchemaType::KBV_PR_FOR_HumanName:
            case SchemaType::KBV_PR_FOR_Organization:
            case SchemaType::KBV_PR_FOR_Patient:
            case SchemaType::KBV_PR_FOR_Practitioner:
            case SchemaType::KBV_PR_FOR_PractitionerRole:
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(
                                type, ::model::ResourceVersion::current<::model::ResourceVersion::KbvItaErp>()) !=
                            nullptr);
                break;
            default:
                ASSERT_TRUE(false) << "unhandled SchemaType";
                break;
        }
    }
}

TEST_F(XmlValidatorTest, MinimalFhirDocumentMustFail)
{
    const auto document = ::boost::str(::boost::format(R"(
<Communication xmlns="http://hl7.org/fhir">
    <meta>
        <profile value="%1%" />
    </ meta>
</ Communication>)") % ::model::ResourceVersion::versionizeProfile(
                                           "https://gematik.de/fhir/StructureDefinition/erxCommunicationDispReq"));
    ASSERT_THROW(FhirConverter().xmlStringToJsonWithValidation(
                     document, *getXmlValidator(), SchemaType::Gem_erxCommunicationDispReq,
                     model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>()),
                 ErpException);
}

struct TestParamsGematik
{
    bool valid;
    fs::path basePath;
    std::string startsWith;
    SchemaType schemaType;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 version;
};
std::ostream& operator<<(std::ostream& os, const TestParamsGematik& params)
{
    os << "should be " << (params.valid?"valid: ":"invalid: ") << magic_enum::enum_name(params.schemaType)
       << " " << params.startsWith;
    return os;
}
struct TestParamsKbv
{
    bool valid;
    fs::path basePath;
    std::string startsWith;
    SchemaType schemaType;
    model::ResourceVersion::KbvItaErp version;
};
std::ostream& operator<<(std::ostream& os, const TestParamsKbv& params)
{
    os << "should be " << (params.valid?"valid: ":"invalid: ") << magic_enum::enum_name(params.schemaType)
       << " " << params.startsWith;
    return os;
}

class XmlValidatorTestParams
{
public:
    std::shared_ptr<XmlValidator> getXmlValidator()
    {
        return StaticData::getXmlValidator();
    }
};
class XmlValidatorTestParamsGematik : public XmlValidatorTestParams, public testing::TestWithParam<TestParamsGematik> {};
class XmlValidatorTestParamsKbv : public XmlValidatorTestParams, public testing::TestWithParam<TestParamsKbv> {};


TEST_P(XmlValidatorTestParamsGematik, Resources)
{
    for (const auto& dirEntry : fs::directory_iterator(GetParam().basePath))
    {
        if (dirEntry.is_regular_file() &&
            String::starts_with(dirEntry.path().filename().string(), GetParam().startsWith))
        {
            const auto document = FileHelper::readFileAsString(dirEntry.path());
            if (GetParam().valid)
            {
                if (GetParam().schemaType == SchemaType::fhir)
                {
                    EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidationNoVer(document, *getXmlValidator(),
                                                                                       GetParam().schemaType))
                        << "Test failed for file: " << dirEntry.path().filename().string();
                }
                else
                {
                    EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
                        document, *getXmlValidator(), GetParam().schemaType, GetParam().version))
                        << "Test failed for file: " << dirEntry.path().filename().string();
                }
            }
            else
            {
                if (GetParam().schemaType == SchemaType::fhir)
                {
                    EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidationNoVer(document, *getXmlValidator(),
                                                                                    GetParam().schemaType),
                                 ErpException)
                        << "Test failed for file: " << dirEntry.path().filename().string();
                }
                else
                {
                    EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(
                                     document, *getXmlValidator(), GetParam().schemaType, GetParam().version),
                                 ErpException)
                        << "Test failed for file: " << dirEntry.path().filename().string();
                }
            }
        }
    }
}


TEST_P(XmlValidatorTestParamsKbv, Resources)
{
    for (const auto& dirEntry : fs::directory_iterator(GetParam().basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), GetParam().startsWith))
        {
            const auto document = FileHelper::readFileAsString(dirEntry.path());
            if (GetParam().valid)
            {
                EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
                    document, *getXmlValidator(), GetParam().schemaType, GetParam().version))
                    << "Test failed for file: " << dirEntry.path().filename().string();
            }
            else
            {
                EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(
                                 document, *getXmlValidator(), GetParam().schemaType, GetParam().version), ErpException)
                    << "Test failed for file: " << dirEntry.path().filename().string();
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ValidResources, XmlValidatorTestParamsGematik,
    testing::Values(
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/", "AuditEvent_valid", SchemaType::Gem_erxAuditEvent, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_valid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/", "CommunicationDispReq_valid", SchemaType::Gem_erxCommunicationDispReq, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/", "CommunicationInfoReq_valid", SchemaType::Gem_erxCommunicationInfoReq, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/", "CommunicationReply_valid", SchemaType::Gem_erxCommunicationReply, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/", "CommunicationRepresentative_valid", SchemaType::Gem_erxCommunicationRepresentative, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/composition/", "Composition_valid", SchemaType::Gem_erxCompositionElement, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_valid", SchemaType::Gem_erxDevice, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/", "MedicationDispense_valid", SchemaType::Gem_erxMedicationDispense, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/organization/", "Organization_valid", SchemaType::Gem_erxOrganizationElement, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "activateTaskParameters_valid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "createTaskParameters_valid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/", "ReceiptBundle_valid", SchemaType::Gem_erxReceiptBundle, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{true, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_valid", SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},

        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/", "AuditEvent_invalid", SchemaType::Gem_erxAuditEvent, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_invalid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/", "CommunicationDispReq_invalid", SchemaType::Gem_erxCommunicationDispReq, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/", "CommunicationInfoReq_invalid", SchemaType::Gem_erxCommunicationInfoReq, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/", "CommunicationReply_invalid", SchemaType::Gem_erxCommunicationReply, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/", "CommunicationRepresentative_invalid", SchemaType::Gem_erxCommunicationRepresentative, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/composition/", "Composition_invalid", SchemaType::Gem_erxCommunicationReply, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_invalid", SchemaType::Gem_erxDevice, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/", "MedicationDispense_invalid", SchemaType::Gem_erxMedicationDispense, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/organization/", "Organization_invalid", SchemaType::Gem_erxOrganizationElement, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "activateTaskParameters_invalid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "createTaskParameters_invalid", SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/", "ReceiptBundle_invalid", SchemaType::Gem_erxReceiptBundle, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1},
        TestParamsGematik{false, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_invalid", SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1}
));

TEST_F(XmlValidatorTest, Erp6345)
{
    auto file1 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile.xml");

    EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(file1, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle,
                                                               model::ResourceVersion::KbvItaErp::v1_0_2),
                 ErpException);

    auto file2 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile_valid.xml");
    EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
        file2, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2));
}

class XmlValidatorTestPeriod : public XmlValidatorTest
{
public:
    void DoTestOnePeriod(std::optional<model::Timestamp> begin, std::optional<model::Timestamp> end, bool expectSuccess)
    {
        const auto& config = Configuration::instance();
        xmlValidator.loadGematikSchemas(
            ::std::string{::model::ResourceVersion::v_str(
                ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>())},
            config.getArray(ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK), begin, end);
        DoTest(expectSuccess);
    }

    void DoTestTwoPeriods(std::optional<model::Timestamp> begin1, std::optional<model::Timestamp> end1,
                          std::optional<model::Timestamp> begin2, std::optional<model::Timestamp> end2,
                          bool expectSuccess)
    {
        const auto& config = Configuration::instance();
        xmlValidator.loadGematikSchemas(
            ::std::string{::model::ResourceVersion::v_str(
                ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>())},
            config.getArray(ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK), begin1, end1);
        xmlValidator.loadGematikSchemas(
            ::std::string{::model::ResourceVersion::v_str(
                ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>())},
            config.getArray(ConfigurationKey::FHIR_PROFILE_XML_SCHEMA_GEMATIK), begin2, end2);
        DoTest(expectSuccess);
    }

    XmlValidator xmlValidator;

private:
    void DoTest(bool expectSuccess)
    {
        auto file_valid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/MedicationDispense_valid.xml");
        auto file_invalid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) /
            "validation/xml/medicationdispense/MedicationDispense_invalid_wrongSubjectIdentifierUse.xml");

        const auto version = ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>();
        if (expectSuccess)
        {
            EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
                file_valid, xmlValidator, SchemaType::Gem_erxMedicationDispense, version));
        }
        else
        {
            EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(file_valid, xmlValidator,
                                                                       SchemaType::Gem_erxMedicationDispense, version),
                         ErpException);
        }
        EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(file_invalid, xmlValidator,
                                                                   SchemaType::Gem_erxMedicationDispense, version),
                     ErpException);
    }
};

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_noPeriod)
{
    DoTestOnePeriod(std::nullopt, std::nullopt, true);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_validPeriod)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + -1min;
    const auto end = model::Timestamp::now() + 1min;
    DoTestOnePeriod(begin, end, true);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_validBegin)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + -1min;
    DoTestOnePeriod(begin, std::nullopt, true);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_validEnd)
{
    using namespace std::chrono_literals;
    const auto end = model::Timestamp::now() + 1min;
    DoTestOnePeriod(std::nullopt, end, true);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_invalidPeriodEnBeforeNow)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + -2min;
    const auto end = model::Timestamp::now() + -1min;
    DoTestOnePeriod(begin, end, false);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_invalidPeriodBeginAfterNow)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + 1min;
    const auto end = model::Timestamp::now() + 2min;
    DoTestOnePeriod(begin, end, false);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_invalidBeginAfterNow)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + 1min;
    DoTestOnePeriod(begin, std::nullopt, false);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_invalidEndBeforeNow)
{
    using namespace std::chrono_literals;
    const auto end = model::Timestamp::now() + -1min;
    DoTestOnePeriod(std::nullopt, end, false);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_valid2Periods)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + -1min;
    const auto end = model::Timestamp::now() + 1min;
    const auto end2 = model::Timestamp::now() + 2min;
    DoTestTwoPeriods(begin, end, end, end2, true);
}

TEST_F(XmlValidatorTestPeriod, ValidityPeriods_invalid2Periods)
{
    using namespace std::chrono_literals;
    const auto begin = model::Timestamp::now() + -2min;
    const auto end = model::Timestamp::now() + -1min;
    const auto begin2 = model::Timestamp::now() + 1min;
    const auto end2 = model::Timestamp::now() + 2min;
    DoTestTwoPeriods(begin, end, begin2, end2, false);
}
