/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/FileHelper.hxx"
#include "test_config.h"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/RegistrationMock.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"

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

	void handleRequest(PcSessionContext& session) override { (void)session; }
    void handleRequest(BaseSessionContext& session) override { handleRequest(dynamic_cast<PcSessionContext&>(session)); }

    template<typename TModel>
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void testParseAndValidateRequestBodyT(std::string body, const std::string& contentMimeType, bool expectFail)
    {
        Header header(HttpMethod::POST, "", Header::Version_1_1, {{Header::ContentType, contentMimeType}}, HttpStatus::Unknown);
        ServerRequest serverRequest(std::move(header));
        serverRequest.setBody(std::move(body));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext(serviceContext, serverRequest, serverResponse, accessLog);
        std::string profileTypeStr = TModel::resourceTypeName;
        if constexpr (model::FhirValidatableProfileConstexpr<TModel>)
        {
            profileTypeStr = magic_enum::enum_name(TModel::profileType);
        }
        if (expectFail)
        {
            ASSERT_ANY_THROW((void) parseAndValidateRequestBody<TModel>(sessionContext))
                << "failed with " << contentMimeType << ", " << profileTypeStr
                << " expectFail=true";
        }
        else
        {
            ASSERT_NO_THROW((void) parseAndValidateRequestBody<TModel>(sessionContext))
                << "failed with " << contentMimeType << ", " << profileTypeStr
                << " expectFail=false";
        }

    }

    void testParseAndValidateRequestBody(const std::string& body, const std::string& contentMimeType,
                                         model::ProfileType profileType, bool expectFail)
    {
        switch (profileType)
        {
            using enum model::ProfileType;
            case GEM_ERP_PR_Communication_DispReq:
            case GEM_ERP_PR_Communication_InfoReq:
            case GEM_ERPCHRG_PR_Communication_ChargChangeReq:
            case GEM_ERPCHRG_PR_Communication_ChargChangeReply:
            case GEM_ERP_PR_Communication_Reply:
            case GEM_ERP_PR_Communication_Representative:
                // parseAndValidateRequestBody not used for Communication
                break;
            case GEM_ERP_PR_MedicationDispense:
            case GEM_ERP_PR_MedicationDispense_DiGA:
                testParseAndValidateRequestBodyT<model::MedicationDispense>(body, contentMimeType, expectFail);
                break;
            case MedicationDispenseBundle:
                testParseAndValidateRequestBodyT<model::MedicationDispenseBundle>(body, contentMimeType, expectFail);
                break;
            case ActivateTaskParameters:
                testParseAndValidateRequestBodyT<model::ActivateTaskParameters>(body, contentMimeType, expectFail);
                break;
            case CreateTaskParameters:
                testParseAndValidateRequestBodyT<model::CreateTaskParameters>(body, contentMimeType, expectFail);
                break;
            case fhir:
            case GEM_ERP_PR_Bundle:
            case GEM_ERP_PR_Task:
            case GEM_ERP_PR_AuditEvent:
            case GEM_ERP_PR_Binary:
            case KBV_PR_ERP_Composition:
            case KBV_PR_ERP_Medication_Compounding:
            case KBV_PR_ERP_Medication_FreeText:
            case KBV_PR_ERP_Medication_Ingredient:
            case KBV_PR_ERP_Medication_PZN:
            case KBV_PR_ERP_PracticeSupply:
            case KBV_PR_ERP_Prescription:
            case KBV_PR_EVDGA_Bundle:
            case KBV_PR_EVDGA_HealthAppRequest:
            case KBV_PR_FOR_Coverage:
            case KBV_PR_FOR_Organization:
            case KBV_PR_FOR_Practitioner:
            case GEM_ERP_PR_Composition:
            case GEM_ERP_PR_Device:
            case GEM_ERP_PR_Digest:
            case GEM_ERP_PR_Medication:
            case GEM_ERP_PR_PAR_CloseOperation_Input:
            case GEM_ERP_PR_PAR_DispenseOperation_Input:
            case KBV_PR_ERP_Bundle:
            case KBV_PR_FOR_PractitionerRole:
            case KBV_PR_FOR_Patient:
            case PatchChargeItemParameters:
            case DAV_PKV_PR_ERP_AbgabedatenBundle:
            case Subscription:
            case OperationOutcome:
            case ProvidePrescriptionErpOp:
            case ProvideDispensationErpOp:
            case EPAOpRxPrescriptionERPOutputParameters:
            case EPAOpRxDispensationERPOutputParameters:
            case CancelPrescriptionErpOp:
            case OrganizationDirectory:
            case EPAMedicationPZNIngredient:
                FAIL() << "wrong SchemaType for this test";
                break;
            case GEM_ERPCHRG_PR_ChargeItem:
                testParseAndValidateRequestBodyT<model::ChargeItem>(body, contentMimeType, expectFail);
                break;
            case GEM_ERPCHRG_PR_Consent:
                testParseAndValidateRequestBodyT<model::Consent>(body, contentMimeType, expectFail);
                break;
        }
    }

    PcServiceContext serviceContext;
};

struct SampleFile {
    std::string_view xmlSample;
    std::string_view jsonSample;
    model::ProfileType profileType;
    std::optional<std::string> skipReason = {};
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
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" / GetParam().jsonSample),
        ContentMimeType::fhirJsonUtf8, GetParam().profileType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" / GetParam().jsonSample),
        ContentMimeType::fhirXmlUtf8, GetParam().profileType, true);
}

TEST_P(ErpRequestHandlerTest, validateRequestBodyReferenceSamplesXML)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->data());
    }
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" / GetParam().xmlSample),
        ContentMimeType::fhirXmlUtf8, GetParam().profileType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" / GetParam().xmlSample),
        ContentMimeType::fhirJsonUtf8, GetParam().profileType, true);
}

using namespace std::string_view_literals;


// Test the resources that are actually supported at this interface.
INSTANTIATE_TEST_SUITE_P(gematikExamples, ErpRequestHandlerTest,
                         ::testing::Values(
                             SampleFile{
                                 "chargeItem_simplifier.xml",
                                 "chargeItem_simplifier.json",
                                 model::ProfileType::GEM_ERPCHRG_PR_ChargeItem,
                             },
                             SampleFile{
                                 "consent_simplifier.xml",
                                 "consent_simplifier.json",
                                 model::ProfileType::GEM_ERPCHRG_PR_Consent,
                             }));
