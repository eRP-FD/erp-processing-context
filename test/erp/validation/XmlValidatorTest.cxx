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
#include <variant>

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
#include "test/util/TestUtils.hxx"
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

    void SetUp() override
    {
        envGuards = testutils::getOverlappingFhirProfileEnvironment();
    }

    void TearDown() override
    {
        envGuards.clear();
    }
private:
    std::vector<EnvironmentVariableGuard> envGuards;
};


TEST_F(XmlValidatorTest, ValidateDefaultTask)
{
    auto envGuards = testutils::getOldFhirProfileEnvironment();
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
    auto envGuards = testutils::getOldFhirProfileEnvironment();
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
    auto envGuards = testutils::getOldFhirProfileEnvironment();
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
    std::variant<model::ResourceVersion::DeGematikErezeptWorkflowR4, model::ResourceVersion::FhirProfileBundleVersion>
        version;
};
std::ostream& operator<<(std::ostream& os, const TestParamsGematik& params)
{
    os << "should be " << (params.valid?"valid: ":"invalid: ") << magic_enum::enum_name(params.schemaType)
       << " " << params.path.filename();
    return os;
}
std::vector<TestParamsGematik> makeTestParameters(
    bool valid, const fs::path& basePath, const std::string& startsWith, SchemaType schemaType,
    std::variant<model::ResourceVersion::DeGematikErezeptWorkflowR4, model::ResourceVersion::FhirProfileBundleVersion>
        version)
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
    fhirtools::ValidationResults
    validate(const model::NumberAsStringParserDocument& doc,
             model::ResourceVersion::FhirProfileBundleVersion version)
    {
        using namespace model::resource;
        fhirtools::ValidationResults result;
        static const rapidjson::Pointer resourceTypePointer{ ElementName::path(elements::resourceType) };
        auto resourceTypeOpt = model::NumberAsStringParserDocument::getOptionalStringValue(doc, resourceTypePointer);
        EXPECT_TRUE(resourceTypeOpt);
        if (!resourceTypeOpt)
        {
            result.add(fhirtools::Severity::error, "failed to detect resource", "", nullptr);
            return result;
        }
        std::string resourceType{*resourceTypeOpt};
        auto erpElement = std::make_shared<ErpElement>(&Fhir::instance().structureRepository(version),
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
            bool unversionedValidation = GetParam().schemaType == SchemaType::fhir ||
                                         GetParam().schemaType == SchemaType::MedicationDispenseBundle;
            model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVer{};
            model::ResourceVersion::FhirProfileBundleVersion bundleVer{};
            if (std::holds_alternative<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version))
            {
                gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version);
                bundleVer = model::ResourceVersion::isProfileSupported(gematikVer).value();
            }
            else
            {
                bundleVer = std::get<model::ResourceVersion::FhirProfileBundleVersion>(GetParam().version);
                gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(profileVersionFromBundle(bundleVer));
            }
            if (GetParam().valid)
            {
                if (unversionedValidation)
                {
                    EXPECT_NO_THROW(doc.emplace(FhirConverter().xmlStringToJsonWithValidationNoVer(
                        document, *getXmlValidator(), GetParam().schemaType)));
                }
                else
                {
                    EXPECT_NO_THROW(doc.emplace(FhirConverter().xmlStringToJsonWithValidation(
                        document, *getXmlValidator(), GetParam().schemaType, gematikVer)));
                }
            }
            else
            {
                if (unversionedValidation)
                {
                    doc.emplace(FhirConverter().xmlStringToJsonWithValidationNoVer(document, *getXmlValidator(),
                                                                                   GetParam().schemaType));
                }
                else
                {
                    doc.emplace(FhirConverter().xmlStringToJsonWithValidation(
                        document, *getXmlValidator(), GetParam().schemaType, gematikVer));
                }
            }
            if (doc)
            {
                fhirtools::ValidationResults valResult;
                ASSERT_NO_THROW(valResult = validate(*doc, bundleVer));
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
    LOG(INFO) << "checking file = " << GetParam().path.filename();
    checkDocument(document);
}

INSTANTIATE_TEST_SUITE_P(
    AuditEventValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/1.1.1/",
                                         "AuditEvent_valid", SchemaType::Gem_erxAuditEvent,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(AuditEventValidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/1.2/", "AuditEvent_valid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(BundleValidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/1.1.1/", "Bundle_valid", SchemaType::fhir,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(BundleValidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/1.2", "Bundle_valid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/1.1.1/",
                                         "CommunicationDispReq_valid", SchemaType::Gem_erxCommunicationDispReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/1.2/",
                                         "CommunicationDispReq_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/1.1.1/",
                                         "CommunicationInfoReq_valid", SchemaType::Gem_erxCommunicationInfoReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/1.2/",
                                         "CommunicationInfoReq_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/1.1.1/",
                                         "CommunicationReply_valid", SchemaType::Gem_erxCommunicationReply,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/1.2/",
                                         "CommunicationReply_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/",
                             "CommunicationRepresentative_valid", SchemaType::Gem_erxCommunicationRepresentative,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeValidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/1.2/",
                             "CommunicationRepresentative_valid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CompositionValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/composition/1.1.1/",
                                         "Composition_valid", SchemaType::Gem_erxCompositionElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(DeviceValidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/device/1.1.1/", "Device_valid",
                             SchemaType::Gem_erxDevice, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)));

INSTANTIATE_TEST_SUITE_P(DeviceValidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/device/1.2/", "Device_valid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/",
                                         "MedicationDispense_valid", SchemaType::Gem_erxMedicationDispense,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.2/",
                                         "MedicationDispense_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/organization/1.1.1/",
                                         "Organization_valid", SchemaType::Gem_erxOrganizationElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/organization/1.2/",
                                         "Organization_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
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
    ReceiptBundleValidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/1.1.1/",
                                         "ReceiptBundle_valid", SchemaType::Gem_erxReceiptBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    ReceiptBundleValidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/1.2/",
                                         "ReceiptBundle_valid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(TaskValidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/task/1.1.1/", "Task_valid",
                             SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(TaskValidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/task/1.2/", "Task_valid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(ChargeItemValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/chargeItem/", "ChargeItem_valid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(ConsentValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/consent/", "Consent_valid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(AbgabedatenBundleValidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             true, fs::path(TEST_DATA_DIR) / "validation/xml/v_2023_07_01/dav/AbgabedatenBundle",
                             "Bundle_valid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(AbgabedatenBundleInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/v_2023_07_01/dav/AbgabedatenBundle",
                             "Bundle_invalid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(
    AuditEventInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/",
                                         "AuditEvent_invalid", SchemaType::Gem_erxAuditEvent,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(AuditEventInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/auditevent/1.2/", "AuditEvent_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(BundleInvalidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/1.1.1/", "Bundle_invalid",
                             SchemaType::fhir, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(BundleInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/bundle/1.2/", "Bundle_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/1.1.1/",
                                         "CommunicationDispReq_invalid", SchemaType::Gem_erxCommunicationDispReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationDispReqInvalidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/dispreq/1.2/",
                                         "CommunicationDispReq_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/1.1.1/",
                                         "CommunicationInfoReq_invalid", SchemaType::Gem_erxCommunicationInfoReq,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationInfoReqInvalidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/inforeq/1.2/",
                                         "CommunicationInfoReq_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/1.1.1",
                                         "CommunicationReply_invalid", SchemaType::Gem_erxCommunicationReply,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    CommunicationReplyInvalidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/reply/1.2/",
                                         "CommunicationReply_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeInvalidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/1.1.1/",
                             "CommunicationRepresentative_invalid", SchemaType::Gem_erxCommunicationRepresentative,
                             model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(CommunicationRepresentativeInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/communication/representative/1.2/",
                             "CommunicationRepresentative_invalid", SchemaType::fhir,
                             model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

INSTANTIATE_TEST_SUITE_P(
    CompositionInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/composition/1.1.1/",
                                         "Composition_invalid", SchemaType::Gem_erxCompositionElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));

INSTANTIATE_TEST_SUITE_P(DeviceInvalidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/device/1.1.1/", "Device_invalid",
                             SchemaType::Gem_erxDevice, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)));

INSTANTIATE_TEST_SUITE_P(DeviceInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/device/1.2/", "Device_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/",
                                         "MedicationDispense_invalid", SchemaType::Gem_erxMedicationDispense,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseInvalidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.2/",
                                         "MedicationDispense_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/organization/1.1.1/",
                                         "Organization_invalid", SchemaType::Gem_erxOrganizationElement,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    OrganizationInvalidResources2023, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/organization/1.2/",
                                         "Organization_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
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
    ReceiptBundleInvalidResources2022, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/1.1.1",
                                         "ReceiptBundle_invalid", SchemaType::Gem_erxReceiptBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(ReceiptBundleInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/receipt/1.2", "ReceiptBundle_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(TaskInvalidResources2022, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/task/1.1.1/", "Task_invalid",
                             SchemaType::Gem_erxTask, model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(TaskInvalidResources2023, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/task/1.2/", "Task_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    ChargeItemInvalidResources, XmlValidatorTestParamsGematik,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/chargeItem/",
                                         "ChargeItem_invalid", SchemaType::fhir,
                                         model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(ConsentInvalidResources, XmlValidatorTestParamsGematik,
                         testing::ValuesIn(makeTestParameters(
                             false, fs::path(TEST_DATA_DIR) / "validation/xml/pkv/consent/", "Consent_invalid",
                             SchemaType::fhir, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));


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

class MedicationDispenseBundleTest : public XmlValidatorTestParamsGematik
{
};

TEST_P(MedicationDispenseBundleTest, MedicationDispenseBundle)
{
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVer{};
    if (std::holds_alternative<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version))
    {
        gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version);
    }
    else
    {
        const auto bundleVer = std::get<model::ResourceVersion::FhirProfileBundleVersion>(GetParam().version);
        gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(profileVersionFromBundle(bundleVer));
    }
    const auto gematikVerString = v_str(gematikVer);
    const std::string placeholderFile = fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense_bundle/" /
                                        gematikVerString / "MedicationDispenseBundle_placeholder.xml";
    const auto& placeholderDocument = FileHelper::readFileAsString(placeholderFile);
    auto document = FileHelper::readFileAsString(GetParam().path);
    document = String::replaceAll(document, " xmlns=\"http://hl7.org/fhir\"", "");
    document = String::replaceAll(document, "<?xml version=\"1.0\" encoding=\"utf-8\"?>", "");
    const auto bundleDocument = String::replaceAll(placeholderDocument, "###PLACEHOLDER###", document);
    checkDocument(bundleDocument);
}

INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleValid2022, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/",
                                      "MedicationDispense_valid", SchemaType::MedicationDispenseBundle,
                                      model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleValid2023, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.2/",
                                      "MedicationDispense_valid", SchemaType::fhir,
                                      model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleInvalid2022, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/",
                                         "MedicationDispense_invalid", SchemaType::MedicationDispenseBundle,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1)));
INSTANTIATE_TEST_SUITE_P(
    MedicationDispenseBundleInvalid2023, MedicationDispenseBundleTest,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.2/",
                                      "MedicationDispense_invalid", SchemaType::fhir,
                                      model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

class CompositionBundleTest : public XmlValidatorTestParamsGematik
{
};

TEST_P(CompositionBundleTest, CompositionBundle)
{
    LOG(INFO) << "testing " << GetParam().path;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVer{};
    if (std::holds_alternative<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version))
    {
        gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(GetParam().version);
    }
    else
    {
        const auto bundleVer = std::get<model::ResourceVersion::FhirProfileBundleVersion>(GetParam().version);
        gematikVer = std::get<model::ResourceVersion::DeGematikErezeptWorkflowR4>(profileVersionFromBundle(bundleVer));
    }
    const auto gematikVerString = v_str(gematikVer);
    const std::string placeholderFile = fs::path(TEST_DATA_DIR) / "validation/xml/composition_bundle/" /
                                        gematikVerString / "CompositionBundle_placeholder.xml";
    const auto& placeholderDocument = FileHelper::readFileAsString(placeholderFile);
    auto document = FileHelper::readFileAsString(GetParam().path);
    document = String::replaceAll(document, " xmlns=\"http://hl7.org/fhir\"", "");
    document = String::replaceAll(document, "<?xml version=\"1.0\" encoding=\"utf-8\"?>", "");
    const auto bundleDocument = String::replaceAll(placeholderDocument, "###PLACEHOLDER###", document);
    checkDocument(bundleDocument);
}

INSTANTIATE_TEST_SUITE_P(
    CompositionBundleValid2023, CompositionBundleTest,
    testing::ValuesIn(makeTestParameters(true, fs::path(TEST_DATA_DIR) / "validation/xml/composition/1.2/",
                                      "Composition_valid", SchemaType::fhir,
                                      model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));
INSTANTIATE_TEST_SUITE_P(
    CompositionBundleInvalid2023, CompositionBundleTest,
    testing::ValuesIn(makeTestParameters(false, fs::path(TEST_DATA_DIR) / "validation/xml/composition/1.2/",
                                      "Composition_invalid", SchemaType::fhir,
                                      model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)));

class XmlValidatorTestPeriod : public XmlValidatorTest
{
public:
    void SetUp() override
    {
        // the tests make only sense for XmlValidation which is only active for
        // the old profiles
        envGuards = testutils::getOldFhirProfileEnvironment();
    }

    void TearDown() override
    {
        envGuards.clear();
    }

    void DoTestOnePeriod(std::optional<model::Timestamp> begin, std::optional<model::Timestamp> end, bool expectSuccess)
    {
        const auto& config = Configuration::instance();
        xmlValidator.loadGematikSchemas(
            ::std::string{::model::ResourceVersion::v_str(
                ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>())},
            config.getArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK), begin, end);
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
            config.getArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK), begin1, end1);
        xmlValidator.loadGematikSchemas(
            ::std::string{::model::ResourceVersion::v_str(
                ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>())},
            config.getArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK), begin2, end2);
        DoTest(expectSuccess);
    }

    XmlValidator xmlValidator;

private:
    void DoTest(bool expectSuccess) const //NOLINT(readability-function-cognitive-complexity)
    {
        auto file_valid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/MedicationDispense_valid.xml");
        auto file_invalid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) /
            "validation/xml/medicationdispense/1.1.1/MedicationDispense_invalid_wrongSubjectIdentifierUse.xml");

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
    std::vector<EnvironmentVariableGuard> envGuards;
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

    EXPECT_THROW((void)model::KbvBundle::fromXml(bundle, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                                           SchemaType::KBV_PR_ERP_Bundle),
                 ExceptionWrapper<ErpException>);
}
