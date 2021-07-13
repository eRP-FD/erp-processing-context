#include "erp/service/ErpRequestHandler.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/StaticData.hxx"

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/Communication.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test_config.h"

#include <gtest/gtest.h>


namespace fs = std::filesystem;

class ErpRequestHandlerUnderTest : public ErpRequestHandler
{
public:
    ErpRequestHandlerUnderTest()
        : ErpRequestHandler(Operation::UNKNOWN, {}),
          serviceContext(
              Configuration::instance(),
              &MockDatabase::createMockDatabase,
              std::make_unique<DosHandler>(std::make_unique<MockRedisStore>()),
              std::make_unique<HsmPool>(
                  std::make_unique<HsmMockFactory>(),
                  TeeTokenUpdater::createMockTeeTokenUpdaterFactory()),
              StaticData::getJsonValidator(),
              StaticData::getXmlValidator(),
              TslTestHelper::createTslManager<TslManager>())
    {
    }

    template <typename TModel>
    void testParseAndValidateRequestBody(std::string body, const std::string& contentMimeType, SchemaType schemaType, bool expectFail)
    {
        Header header(HttpMethod::POST, "", Header::Version_1_1, {{Header::ContentType, contentMimeType}}, HttpStatus::Unknown, true);
        ServerRequest serverRequest(std::move(header));
        serverRequest.setBody(std::move(body));
        ServerResponse serverResponse;
        SessionContext<PcServiceContext> sessionContext(serviceContext, serverRequest, serverResponse);
        if (expectFail)
        {
            ASSERT_ANY_THROW((void)parseAndValidateRequestBody<TModel>(sessionContext, schemaType))
                << "failed with " << contentMimeType << ", " << magic_enum::enum_name(schemaType) << " expectFail=true";
        }
        else
        {
            ASSERT_NO_THROW((void) parseAndValidateRequestBody<TModel>(sessionContext, schemaType))
                << "failed with " << contentMimeType << ", " << magic_enum::enum_name(schemaType) << " expectFail=false";
        }
    }

    void handleRequest(SessionContext<PcServiceContext>&) override
    {
    }

    PcServiceContext serviceContext;
};

struct SampleFile {
    std::string_view xmlSample;
    std::string_view jsonSample;
    SchemaType schemaType;
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

    virtual void SetUp() override
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
        mErpRequestHandlerUnderTest = std::make_unique<ErpRequestHandlerUnderTest>();
    }

    virtual void TearDown() override
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
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody<model::Communication>(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().jsonSample), ContentMimeType::fhirJsonUtf8, GetParam().schemaType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody<model::Communication>(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().jsonSample), ContentMimeType::fhirXmlUtf8, GetParam().schemaType, true);
}

TEST_P(ErpRequestHandlerTest, validateRequestBodyReferenceSamplesXML)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->data());
    }
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody<model::Communication>(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().xmlSample), ContentMimeType::fhirXmlUtf8, GetParam().schemaType, false);

    // check that it does not validate using wrong mime type
    mErpRequestHandlerUnderTest->testParseAndValidateRequestBody<model::Communication>(
        FileHelper::readFileAsString(fs::path(TEST_DATA_DIR) / "fhir/conversion" /
            GetParam().xmlSample), ContentMimeType::fhirJsonUtf8, GetParam().schemaType, true);
}

using namespace std::string_view_literals;

INSTANTIATE_TEST_SUITE_P(
    gematikExamples, ErpRequestHandlerTest,
    ::testing::Values(
        SampleFile{"audit_event.xml", "audit_event.json", SchemaType::Gem_erxAuditEvent},
        SampleFile{"communication_dispense_req.xml", "communication_dispense_req.json",
                   SchemaType::Gem_erxCommunicationDispReq},
        SampleFile{"communication_info_req.xml", "communication_info_req.json",
                   SchemaType::Gem_erxCommunicationInfoReq},
        SampleFile{"communication_reply.xml", "communication_reply.json", SchemaType::Gem_erxCommunicationReply},
        SampleFile{"composition.xml", "composition.json", SchemaType::KBV_PR_ERP_Composition},
        SampleFile{"coverage.xml", "coverage.json", SchemaType::KBV_PR_FOR_Coverage, "ERP-5559: coding system for type"},
        SampleFile{"device.xml", "device.json", SchemaType::Gem_erxDevice},
        SampleFile{"erx_bundle.xml", "erx_bundle.json", SchemaType::Gem_erxReceiptBundle},
        SampleFile{"erx_composition.xml", "erx_composition.json", SchemaType::Gem_erxCompositionElement},
        SampleFile{"kbv_bundle.xml", "kbv_bundle.json", SchemaType::KBV_PR_ERP_Bundle,
                   "ERP-5559: coding system for Coverage.type"},
        SampleFile{"medication_compounding.xml", "medication_compounding.json",
                   SchemaType::KBV_PR_ERP_Medication_Compounding},
        SampleFile{"medication_dispense.xml", "medication_dispense.json", SchemaType::Gem_erxMedicationDispense},
        SampleFile{"medication_free_text.xml", "medication_free_text.json", SchemaType::KBV_PR_ERP_Medication_FreeText},
        SampleFile{"medication_ingredient.xml", "medication_ingredient.json",
                   SchemaType::KBV_PR_ERP_Medication_Ingredient},
        SampleFile{"medication_pzn.xml", "medication_pzn.json", SchemaType::KBV_PR_ERP_Medication_PZN},
        SampleFile{"medication_request.xml", "medication_request.json", SchemaType::KBV_PR_ERP_Prescription},
        SampleFile{"organization.xml", "organization.json", SchemaType::KBV_PR_FOR_Organization},
        SampleFile{"patient.xml", "patient.json", SchemaType::KBV_PR_FOR_Patient},
        SampleFile{"practitioner.xml", "practitioner.json", SchemaType::KBV_PR_FOR_Practitioner},
        SampleFile{"practitioner_role.xml", "practitioner_role.json", SchemaType::KBV_PR_FOR_PractitionerRole},
        SampleFile{"task_activate_parameters.xml", "task_activate_parameters.json", SchemaType::ActivateTaskParameters,
                   "No JSON schema"},
        SampleFile{"task_create_parameters.xml", "task_create_parameters.json", SchemaType::CreateTaskParameters,
                   "No JSON schema"},
        SampleFile{"task.xml", "task.json", SchemaType::Gem_erxTask},
        SampleFile{"task_no_secret.xml", "task_no_secret.json", SchemaType::Gem_erxTask}));
