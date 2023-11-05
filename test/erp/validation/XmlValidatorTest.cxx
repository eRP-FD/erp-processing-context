/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/validation/XmlValidator.hxx"
#include "test/util/StaticData.hxx"

#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <magic_enum.hpp>
#include <ostream>
#include <variant>

#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/ResourceFactory.hxx"
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
#include "fhirtools/util/SaxHandler.hxx"


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
    using TaskFactory = model::ResourceFactory<model::Task>;
    auto envGuards = testutils::getOldFhirProfileEnvironment();
    auto xmlDocument = validTask().serializeToXmlString();
    std::optional<TaskFactory> taskFactory;
    ASSERT_NO_THROW(taskFactory.emplace(TaskFactory::fromXml(xmlDocument, *getXmlValidator())));
    ASSERT_NO_THROW(taskFactory->validateLegacyXSD(SchemaType::Gem_erxTask, *getXmlValidator()));
}

TEST_F(XmlValidatorTest, TaskInvalidPrescriptionId)
{
    using TaskFactory = model::ResourceFactory<model::Task>;
    GTEST_SKIP_("Prescription ID pattern not checked by xsd");
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer prescriptionIdPointer("/identifier/0/value");
    prescriptionIdPointer.Set(jsonDocument, "invalid_id");
    std::optional<TaskFactory> taskFactory;
    ASSERT_NO_THROW(taskFactory.emplace(TaskFactory::fromXml(task.serializeToXmlString(), *getXmlValidator())));
    ASSERT_NO_THROW(taskFactory->validateLegacyXSD(SchemaType::Gem_erxTask, *getXmlValidator()));
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
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(type) != nullptr);
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
                ASSERT_TRUE( getXmlValidator()->getSchemaValidationContext(type) != nullptr);
                break;
            case SchemaType::KBV_PR_ERP_Bundle:
            case SchemaType::KBV_PR_ERP_Composition:
            case SchemaType::KBV_PR_ERP_Medication_BundleDummy:
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
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(type) != nullptr);
                break;
            case SchemaType::ActivateTaskParameters:
            case SchemaType::CreateTaskParameters:
            case SchemaType::MedicationDispenseBundle:
                ASSERT_TRUE(getXmlValidator()->getSchemaValidationContext(type) != nullptr);
                break;
            case SchemaType::PatchChargeItemParameters:
            case SchemaType::DAV_DispenseItem:
            case SchemaType::Pruefungsnachweis:
            case SchemaType::Gem_erxChargeItem:
            case SchemaType::Gem_erxConsent:
            case SchemaType::CommunicationDispReqPayload:
            case SchemaType::CommunicationReplyPayload:
                // not validated with XSD
                break;
            default:
                FAIL() << "unhandled SchemaType";
                break;
        }
    }
}

TEST_F(XmlValidatorTest, MinimalFhirDocumentMustFail)
{
    using CommunicationsFactory = model::ResourceFactory<model::Communication>;
    auto envGuards = testutils::getOldFhirProfileEnvironment();
    const auto document = ::boost::str(::boost::format(R"(
<Communication xmlns="http://hl7.org/fhir">
    <meta>
        <profile value="%1%" />
    </ meta>
</ Communication>)") % ::model::ResourceVersion::versionizeProfile(
                                           "https://gematik.de/fhir/StructureDefinition/erxCommunicationDispReq"));
    ASSERT_THROW(CommunicationsFactory::fromXml(document, *getXmlValidator())
                    .getValidated(SchemaType::Gem_erxCommunicationDispReq, *getXmlValidator(), *StaticData::getInCodeValidator(),
                         {model::ResourceVersion::currentBundle()}),
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
        auto fhirContext = getXmlValidator()->getSchemaValidationContext(SchemaType::fhir);
        ASSERT_NE(fhirContext, nullptr);
        std::optional<model::NumberAsStringParserDocument> doc;
        bool failed = false;
        try
        {
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
            auto context = getXmlValidator()->getSchemaValidationContext(GetParam().schemaType);
            ASSERT_NE(context, nullptr);
            fhirtools::SaxHandler{}.validateStringView(document, *fhirContext);
            fhirtools::SaxHandler{}.validateStringView(document, *context);
            auto doc = Fhir::instance().converter().xmlStringToJson(document);
            fhirtools::ValidationResults valResult;
            ASSERT_NO_THROW(valResult = validate(doc, bundleVer));
            failed = valResult.highestSeverity() >= fhirtools::Severity::error;
        }
        catch (const std::exception& e)
        {
            failed = true;
        }
        EXPECT_EQ(failed, ! GetParam().valid);
    }
    std::vector<EnvironmentVariableGuard> envGuards  = testutils::getOverlappingFhirProfileEnvironment();
};


TEST_P(XmlValidatorTestParamsGematik, Resources)
{
    const auto document = FileHelper::readFileAsString(GetParam().path);
    TLOG(INFO) << "checking file = " << GetParam().path.filename();
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
    using BundleFactory = model::ResourceFactory<model::KbvBundle>;
    auto file1 =
        FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) / "issues/ERP-6345/bundleFromSignedFile.xml");
    std::optional<BundleFactory> factory1;
    ASSERT_NO_THROW(factory1.emplace(BundleFactory::fromXml(file1, *getXmlValidator())));
    EXPECT_THROW(factory1->validateLegacyXSD(SchemaType::KBV_PR_ERP_Bundle, *getXmlValidator()), ErpException);

    auto file2 = FileHelper::readFileAsString(std::filesystem::path(TEST_DATA_DIR) /
                                              "issues/ERP-6345/bundleFromSignedFile_valid.xml");
    std::optional<BundleFactory> factory2;
    ASSERT_NO_THROW(factory2.emplace(BundleFactory::fromXml(file2, *getXmlValidator())));
    EXPECT_NO_THROW(factory2->validateLegacyXSD(SchemaType::KBV_PR_ERP_Bundle, *getXmlValidator()));
}

class MedicationDispenseBundleTest : public XmlValidatorTestParamsGematik
{
    std::vector<EnvironmentVariableGuard> envGuards  = testutils::getOverlappingFhirProfileEnvironment();
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
    TLOG(INFO) << "testing " << GetParam().path;
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
        xmlValidator.loadSchemas(config.getArray(ConfigurationKey::FHIR_PROFILE_OLD_XML_SCHEMA_GEMATIK), begin, end);
        DoTest(expectSuccess);
    }

    XmlValidator xmlValidator;

private:
    void DoTest(bool expectSuccess) const //NOLINT(readability-function-cognitive-complexity)
    {
        using MedicationDispenseFactory = model::ResourceFactory<model::MedicationDispense>;
        auto file_valid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) / "validation/xml/medicationdispense/1.1.1/MedicationDispense_valid.xml");
        auto file_invalid = FileHelper::readFileAsString(
            std::filesystem::path(TEST_DATA_DIR) /
            "validation/xml/medicationdispense/1.1.1/MedicationDispense_invalid_wrongSubjectIdentifierUse.xml");

        if (expectSuccess)
        {
            EXPECT_NO_THROW(MedicationDispenseFactory::fromXml(file_valid, *StaticData::getXmlValidator())
                    .getValidated(SchemaType::Gem_erxMedicationDispense, xmlValidator, *StaticData::getInCodeValidator(),
                         model::ResourceVersion::allBundles()));
        }
        else
        {
            EXPECT_THROW(MedicationDispenseFactory::fromXml(file_valid, *StaticData::getXmlValidator())
                    .getValidated(SchemaType::Gem_erxMedicationDispense, xmlValidator, *StaticData::getInCodeValidator(),
                        model::ResourceVersion::allBundles())
                , ErpException);
        }
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

// This test is only possible when two different profile versions are configured.
TEST_F(XmlValidatorTest, VersionMixup)
{
    using namespace ::std::chrono_literals;
    const auto tomorrow = (::model::Timestamp::now() + 24h).toXsDate(model::Timestamp::GermanTimezone);
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
