/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/ErpRequestHandler.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/StaticData.hxx"

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test_config.h"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


namespace fs = std::filesystem;

class ErpRequestHandlerUnderTest : public ErpRequestHandler
{
public:
    ErpRequestHandlerUnderTest()
        : ErpRequestHandler(Operation::UNKNOWN, {}),
          serviceContext(Configuration::instance(), StaticData::makeMockFactories())
    {
    }

    template <typename TModel>
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void testParseAndValidateRequestBodyT(std::string body, const std::string& contentMimeType, SchemaType schemaType, bool expectFail)
    {
        Header header(HttpMethod::POST, "", Header::Version_1_1, {{Header::ContentType, contentMimeType}}, HttpStatus::Unknown);
        ServerRequest serverRequest(std::move(header));
        serverRequest.setBody(std::move(body));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext(serviceContext, serverRequest, serverResponse, accessLog);
        if (expectFail)
        {
            ASSERT_ANY_THROW((void)parseAndValidateRequestBody<TModel>(sessionContext, schemaType))
                << "failed with " << contentMimeType << ", " << magic_enum::enum_name(schemaType) << " expectFail=true";
        }
        else
        {
            if constexpr (std::is_same_v<TModel, model::Communication>)
            {
                ASSERT_NO_THROW((void) parseAndValidateRequestBody<TModel>(sessionContext, schemaType, false))
                    << "failed with " << contentMimeType << ", " << magic_enum::enum_name(schemaType) << " expectFail=false";
            }
            else
            {
                ASSERT_NO_THROW((void) parseAndValidateRequestBody<TModel>(sessionContext, schemaType))
                    << "failed with " << contentMimeType << ", " << magic_enum::enum_name(schemaType) << " expectFail=false";
            }
        }

    }

    void testParseAndValidateRequestBody(const std::string& body, const std::string& contentMimeType, SchemaType schemaType, bool expectFail)
    {
        switch (schemaType)
        {
            case SchemaType::Gem_erxCommunicationDispReq:
            case SchemaType::Gem_erxCommunicationInfoReq:
            case SchemaType::Gem_erxCommunicationChargChangeReq:
            case SchemaType::Gem_erxCommunicationChargChangeReply:
            case SchemaType::Gem_erxCommunicationReply:
            case SchemaType::Gem_erxCommunicationRepresentative:
                testParseAndValidateRequestBodyT<model::Communication>(body, contentMimeType, schemaType, expectFail);
                break;
            case SchemaType::Gem_erxMedicationDispense:
                testParseAndValidateRequestBodyT<model::MedicationDispense>(body, contentMimeType, schemaType, expectFail);
                break;
            case SchemaType::MedicationDispenseBundle:
                testParseAndValidateRequestBodyT<model::MedicationDispenseBundle>(body, contentMimeType, SchemaType::fhir, expectFail);
                break;
            case SchemaType::ActivateTaskParameters:
            case SchemaType::CreateTaskParameters:
                testParseAndValidateRequestBodyT<model::Parameters>(body, contentMimeType, schemaType, expectFail);
                break;
            case SchemaType::fhir:
            case SchemaType::Gem_erxOrganizationElement:
            case SchemaType::Gem_erxReceiptBundle:
            case SchemaType::Gem_erxTask:
            case SchemaType::Gem_erxAuditEvent:
            case SchemaType::Gem_erxBinary:
            case SchemaType::BNA_tsl:
            case SchemaType::Gematik_tsl:
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
            case SchemaType::KBV_PR_FOR_Practitioner:
            case SchemaType::Gem_erxCompositionElement:
            case SchemaType::Gem_erxDevice:
            case SchemaType::KBV_PR_ERP_Bundle:
            case SchemaType::KBV_PR_FOR_PractitionerRole:
            case SchemaType::KBV_PR_FOR_Patient:
            case SchemaType::PatchChargeItemParameters:
            case SchemaType::DAV_DispenseItem:
            case SchemaType::Pruefungsnachweis:

                ASSERT_TRUE(false) << "wrong SchemaType for this test";
                break;
            case SchemaType::Gem_erxChargeItem:
                testParseAndValidateRequestBodyT<model::ChargeItem>(body, contentMimeType, schemaType, expectFail);
                break;
            case SchemaType::Gem_erxConsent:
                testParseAndValidateRequestBodyT<model::Consent>(body, contentMimeType, schemaType, expectFail);
                break;
        }
    }

    void handleRequest(SessionContext&) override
    {
    }

    PcServiceContext serviceContext;
};

struct SampleFile {
    std::string_view xmlSample;
    std::string_view jsonSample;
    SchemaType schemaType;
    model::ResourceVersion::FhirProfileBundleVersion fhirBundleVersion;
    std::optional<std::string_view> skipReason = {};
};

static std::ostream& operator << (std::ostream& out, const SampleFile& sample)
{
    out << R"({ "xmlSample": ")" << sample.xmlSample;
    out << R"(", "jsonSample": ")" << sample.jsonSample;
    if (sample.skipReason)
    {
        out << R"(", "skipReason": ")" << sample.skipReason.value();
    }
    out << R"(" })";
    return out;
}

class ErpRequestHandlerTest : public ::testing::TestWithParam<SampleFile>
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;
    std::unique_ptr<ErpRequestHandlerUnderTest> mErpRequestHandlerUnderTest;

    void SetUp() override
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        mErpRequestHandlerUnderTest = std::make_unique<ErpRequestHandlerUnderTest>();
    }

    void TearDown() override
    {
        mCaDerPathGuard.reset();
    }
};


TEST_P(ErpRequestHandlerTest, validateRequestBodyReferenceSamplesJSON)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->data());
    }
    if (! model::ResourceVersion::supportedBundles().contains(GetParam().fhirBundleVersion))
    {
        GTEST_SKIP_("Skipping unsupported data");
    }
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().jsonSample), ContentMimeType::fhirJsonUtf8, GetParam().schemaType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().jsonSample), ContentMimeType::fhirXmlUtf8, GetParam().schemaType, true);
}

TEST_P(ErpRequestHandlerTest, validateRequestBodyReferenceSamplesXML)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->data());
    }
    if (! model::ResourceVersion::supportedBundles().contains(GetParam().fhirBundleVersion))
    {
        GTEST_SKIP_("Skipping unsupported data");
    }
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().xmlSample), ContentMimeType::fhirXmlUtf8, GetParam().schemaType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().xmlSample), ContentMimeType::fhirJsonUtf8, GetParam().schemaType, true);
}

using namespace std::string_view_literals;


// Test the resources that are actually supported at this interface.
INSTANTIATE_TEST_SUITE_P(
    gematikExamples, ErpRequestHandlerTest,
    ::testing::Values(
        SampleFile{"communication_dispense_req.xml", "communication_dispense_req.json",
                   SchemaType::Gem_erxCommunicationDispReq, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"communication_info_req.xml", "communication_info_req.json",
                   SchemaType::Gem_erxCommunicationInfoReq, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"communication_reply.xml", "communication_reply.json", SchemaType::Gem_erxCommunicationReply, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"medication_dispense.xml", "medication_dispense.json", SchemaType::Gem_erxMedicationDispense, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"task_activate_parameters.xml", "task_activate_parameters.json", SchemaType::ActivateTaskParameters, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"task_create_parameters.xml", "task_create_parameters.json", SchemaType::CreateTaskParameters, model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
        SampleFile{"chargeItem_simplifier.xml", "chargeItem_simplifier.json", SchemaType::Gem_erxChargeItem, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01},
        SampleFile{"consent_simplifier.xml", "consent_simplifier.json", SchemaType::Gem_erxConsent, model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01}
));
