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
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"

#include "test_config.h"
#include "test/util/ResourceManager.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include <erp/fhir/Fhir.hxx>


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


TEST_F(XmlValidatorTest, getSchemaValidationContext)//NOLINT(readability-function-cognitive-complexity)
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
            case SchemaType::Gem_erxCommunicationChargChangeReq:
            case SchemaType::Gem_erxCommunicationChargChangeReply:
            case SchemaType::Gem_erxCommunicationReply:
            case SchemaType::Gem_erxCommunicationRepresentative:
            case SchemaType::Gem_erxCompositionElement:
            case SchemaType::Gem_erxDevice:
            case SchemaType::Gem_erxMedicationDispense:
            case SchemaType::Gem_erxOrganizationElement:
            case SchemaType::Gem_erxReceiptBundle:
            case SchemaType::Gem_erxTask:
            case SchemaType::Gem_erxChargeItem:
            case SchemaType::Gem_erxConsent:
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
            case SchemaType::ActivateTaskParameters:
            case SchemaType::CreateTaskParameters:
            case SchemaType::MedicationDispenseBundle:
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(
                                type, ::model::ResourceVersion::NotProfiled{}) !=
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
    fs::path path;
    SchemaType schemaType;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 version;
};
std::ostream& operator<<(std::ostream& os, const TestParamsGematik& params)
{
    os << "should be " << (params.valid?"valid: ":"invalid: ") << magic_enum::enum_name(params.schemaType)
       << " " << params.path.filename();
    return os;
}
std::vector<TestParamsGematik> makeTestParameters(bool valid, const fs::path& basePath, const std::string& startsWith,
                                                  SchemaType schemaType,
                                                  model::ResourceVersion::DeGematikErezeptWorkflowR4 version)
{
    std::vector<TestParamsGematik> params;
    for (const auto& dirEntry : fs::directory_iterator(basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), startsWith))
        {
            params.emplace_back(TestParamsGematik{.valid = valid,
                                                  .path = dirEntry.path(),
                                                  .schemaType = schemaType,
                                                  .version = version});
        }
    }
    return params;
}

class XmlValidatorTestParams
{
public:
    std::shared_ptr<XmlValidator> getXmlValidator()
    {
        return StaticData::getXmlValidator();
    }
};
class XmlValidatorTestParamsGematik : public XmlValidatorTestParams, public testing::TestWithParam<TestParamsGematik> {
public:
    fhirtools::ValidationResultList validate(const model::NumberAsStringParserDocument& doc)
    {
        using namespace model::resource;
        fhirtools::ValidationResultList result;
        static const rapidjson::Pointer resourceTypePointer{ ElementName::path(elements::resourceType) };
        auto resourceTypeOpt = model::NumberAsStringParserDocument::getOptionalStringValue(doc, resourceTypePointer);
        EXPECT_TRUE(resourceTypeOpt);
        if (!resourceTypeOpt)
        {
            result.add(fhirtools::Severity::error, "failed to detect resource", "", nullptr);
            return result;
        }
        std::string resourceType{*resourceTypeOpt};
        auto erpElement = std::make_shared<ErpElement>(&Fhir::instance().structureRepository(),
                                                       std::weak_ptr<fhirtools::Element>{}, resourceType, &doc);
        result = fhirtools::FhirPathValidator::validate(erpElement, resourceType);
        result.dumpToLog();
        return result;
    }

    void checkDocument(const std::string& document)
    {
        std::optional<model::NumberAsStringParserDocument> doc;
        bool failed = false;
        try
        {
            if (GetParam().valid)
            {
                if (GetParam().schemaType == SchemaType::fhir ||
                    GetParam().schemaType == SchemaType::MedicationDispenseBundle)
                {
                    EXPECT_NO_THROW(doc.emplace(FhirConverter().xmlStringToJsonWithValidationNoVer(
                        document, *getXmlValidator(), GetParam().schemaType)));
                }
                else
                {
                    EXPECT_NO_THROW(doc.emplace(FhirConverter().xmlStringToJsonWithValidation(
                        document, *getXmlValidator(), GetParam().schemaType, GetParam().version)));
                }
            }
            else
            {
                if (GetParam().schemaType == SchemaType::fhir ||
                    GetParam().schemaType == SchemaType::MedicationDispenseBundle)
                {
                    doc.emplace(FhirConverter().xmlStringToJsonWithValidationNoVer(document, *getXmlValidator(),
                                                                                   GetParam().schemaType));
                }
                else
                {
                    doc.emplace(FhirConverter().xmlStringToJsonWithValidation(
                        document, *getXmlValidator(), GetParam().schemaType, GetParam().version));
                }
            }
            if (doc)
            {
                fhirtools::ValidationResultList valResult;
                ASSERT_NO_THROW(valResult = validate(*doc));
                failed = valResult.highestSeverity() >= fhirtools::Severity::error;
            }
        }
        catch (const std::exception& e)
        {
            failed = true;
        }
        EXPECT_EQ(failed, ! GetParam().valid);
    }
};


TEST_P(XmlValidatorTestParamsGematik, Resources)
{
    const auto document = FileHelper::readFileAsString(GetParam().path);
    checkDocument(document);
}

INSTANTIATE_TEST_SUITE_P(
    AuditEventValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/",
                                         "AuditEvent_valid", SchemaType::Gem_erxAuditEvent,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(BundleValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_valid", SchemaType::fhir,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/",
                                         "CommunicationDispReq_valid", SchemaType::Gem_erxCommunicationDispReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/",
                                         "CommunicationInfoReq_valid", SchemaType::Gem_erxCommunicationInfoReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/",
                                         "CommunicationReply_valid", SchemaType::Gem_erxCommunicationReply,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/",
                             "CommunicationRepresentative_valid", SchemaType::Gem_erxCommunicationRepresentative,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CompositionValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/composition/",
                                         "Composition_valid", SchemaType::Gem_erxCompositionElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(DeviceValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_valid",
                             SchemaType::Gem_erxDevice, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/",
                                         "MedicationDispense_valid", SchemaType::Gem_erxMedicationDispense,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/organization/",
                                         "Organization_valid", SchemaType::Gem_erxOrganizationElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    activateTaskParametersValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/",
                                         "activateTaskParameters_valid", SchemaType::fhir,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(createTaskParametersValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/", "createTaskParameters_valid",
                             SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    ReceiptBundleValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/",
                                         "ReceiptBundle_valid", SchemaType::Gem_erxReceiptBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(TaskValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_valid",
                             SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    ChargeItemValidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/chargeItem/",
                                         "ChargeItem_valid", SchemaType::Gem_erxChargeItem,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(ConsentValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/consent/", "Consent_valid",
                             SchemaType::Gem_erxConsent, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));


INSTANTIATE_TEST_SUITE_P(
    AuditEventInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/",
                                         "AuditEvent_invalid", SchemaType::Gem_erxAuditEvent,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(BundleInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/", "Bundle_invalid",
                             SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/",
                                         "CommunicationDispReq_invalid", SchemaType::Gem_erxCommunicationDispReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/",
                                         "CommunicationInfoReq_invalid", SchemaType::Gem_erxCommunicationInfoReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/",
                                         "CommunicationReply_invalid", SchemaType::Gem_erxCommunicationReply,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/",
                             "CommunicationRepresentative_invalid", SchemaType::Gem_erxCommunicationRepresentative,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CompositionInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/composition/",
                                         "Composition_invalid", SchemaType::Gem_erxCompositionElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(DeviceInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/device/", "Device_invalid",
                             SchemaType::Gem_erxDevice, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/",
                                         "MedicationDispense_invalid", SchemaType::Gem_erxMedicationDispense,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/organization/",
                                         "Organization_invalid", SchemaType::Gem_erxOrganizationElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    activateTaskParametersInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/",
                                         "activateTaskParameters_invalid", SchemaType::fhir,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    createTaskParametersInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/parameters/",
                                         "createTaskParameters_invalid", SchemaType::fhir,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    ReceiptBundleInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/",
                                         "ReceiptBundle_invalid", SchemaType::Gem_erxReceiptBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(TaskInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/task/", "Task_invalid",
                             SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    ChargeItemInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/chargeItem/",
                                         "ChargeItem_invalid", SchemaType::Gem_erxChargeItem,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(ConsentInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/consent/", "Consent_invalid",
                             SchemaType::Gem_erxConsent, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));


TEST_F(XmlValidatorTest, Erp6345)//NOLINT(readability-function-cognitive-complexity)
{
    auto file1 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile.xml");

    EXPECT_THROW(FhirConverter().xmlStringToJsonWithValidation(file1, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle,
                                                               model::ResourceVersion::KbvItaErp::v1_0_2),
                 ErpException);

    auto file2 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile_valid.xml");
    EXPECT_NO_THROW(FhirConverter().xmlStringToJsonWithValidation(
        file2, *getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle, model::ResourceVersion::KbvItaErp::v1_0_2));
}

class MedicationDispenseBundleTest:public XmlValidatorTestParamsGematik{};

TEST_P(MedicationDispenseBundleTest, MedicationDispenseBundle)
{
    const auto& placeholderDocument = FileHelper::readFileAsString(
        fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense_bundle/MedicationDispenseBundle_placeholder.xml");
            auto document = FileHelper::readFileAsString(GetParam().path);
            document = String::replaceAll(document, " xmlns=\"http://hl7.org/fhir\"", "");
            document = String::replaceAll(document, "<?xml version=\"1.0\" encoding=\"utf-8\"?>", "");
            const auto bundleDocument = String::replaceAll(placeholderDocument, "###PLACEHOLDER###", document);
            checkDocument(bundleDocument);
}

INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleValid, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/",
                                      "MedicationDispense_valid", SchemaType::MedicationDispenseBundle,
                                      model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleInvalid, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/",
                                         "MedicationDispense_invalid", SchemaType::MedicationDispenseBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

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
    void DoTest(bool expectSuccess) const //NOLINT(readability-function-cognitive-complexity)
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

// This test is only possible when two different profile versions are configured.
TEST_F(XmlValidatorTest, VersionMixup)
{
    using namespace ::std::chrono_literals;
    const auto tomorrow = (::model::Timestamp::now() + 24h).toXsDateTime();
    EnvironmentVariableGuard varGuard("ERP_FHIR_PROFILE_OLD_VALID_UNTIL", tomorrow);
    // <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText|1.0.2" />
    auto bundle =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_valid_MedicationFreeText.xml");
    bundle = String::replaceAll(bundle, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText|1.0.2",
                                "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText|1.0.1");

    EXPECT_THROW(model::KbvBundle::fromXml(bundle, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                                           SchemaType::KBV_PR_ERP_Bundle),
                 ExceptionWrapper<ErpException>);
}
