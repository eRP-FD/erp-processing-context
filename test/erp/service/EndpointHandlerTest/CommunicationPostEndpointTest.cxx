/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */
// clang-format off
// include of gtest must not be ordered after TelematicPseudonymManager
#include <gtest/gtest.h>
// clang-format on

#include "erp/model/Communication.hxx"
#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/service/CommunicationPostHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/TestUtils.hxx"


struct CommunicationPostEndpointTestOptions {
    HttpStatus expectedResult{HttpStatus::Created};
    std::optional<std::string> message = std::nullopt;
    std::optional<std::string> diagnostics = std::nullopt;
};

class CommunicationPostEndpointTest : public EndpointHandlerTest
{
protected:
    static inline const std::string kvnr{"X123456789"};
    void test(std::string body, const CommunicationPostEndpointTestOptions& opt = {})
    {
        CommunicationPostHandler handler{};
        // invalidate all keys to force refresh during `POST /Communication`
        Header requestHeader{HttpMethod::POST,
                             "/Communication",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                             HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        serverRequest.setBody(body);
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        if (opt.expectedResult == HttpStatus::Created)
        {
            ASSERT_NO_THROW(handler.handleRequest(sessionContext));
            ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
        }
        else
        {

            try
            {
                handler.handleRequest(sessionContext);
                GTEST_FAIL() << "should throw.";
            }
            catch (const ErpException& erpException)
            {
                ASSERT_EQ(erpException.status(), opt.expectedResult);
                if (opt.message)
                {
                    ASSERT_EQ(std::string{erpException.what()}, opt.message.value());
                }
                if (opt.diagnostics)
                {
                    ASSERT_TRUE(erpException.diagnostics().has_value());
                    ASSERT_EQ(erpException.diagnostics().value(), opt.diagnostics.value());
                }
            }
        }
    }
};


TEST_F(CommunicationPostEndpointTest, ERP_12846_SubscriptionKeyRefresh)
{
    auto body = CommunicationJsonStringBuilder{model::Communication::MessageType::InfoReq}
                    .setPrescriptionId(model::PrescriptionId::fromDatabaseId(
                                           model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                                           .toString())
                    .setSender(ActorRole::Insurant, kvnr)
                    .setRecipient(ActorRole::Doctor, "A-Doctor")
                    .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
                    .setPayload("Haben sie das da?")
                    .createJsonString();
    // invalidate all keys to force refresh during `POST /Communication`
    auto& telematicPseudonymManager = mServiceContext.getTelematicPseudonymManager();
    telematicPseudonymManager.mKey1Start = {};
    telematicPseudonymManager.mKey1End = {};
    telematicPseudonymManager.mKey2Start = {};
    telematicPseudonymManager.mKey2End = {};
    ASSERT_NO_FATAL_FAILURE(test(std::move(body)));
}

TEST_F(CommunicationPostEndpointTest, ERP_13187_POST_Communication_InvalidProfile)
{
    namespace rv = model::ResourceVersion;
    using fhirBundle = rv::FhirProfileBundleVersion;
    char message[] = "Invalid request body";
    // clang-format off
    std::string deprecatedProfs =
        R"("https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq|1.1.1", )"
        R"("https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply|1.1.1", )"
        R"("https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq|1.1.1", )"
        R"("https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative|1.1.1")";
    std::string newProfs =
        R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_InfoReq|1.2", )"
        R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply|1.2", )"
        R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq|1.2", )"
        R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Representative|1.2", )"
        R"("https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReq|1.0", )"
        R"("https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReply|1.0")";
    // clang-format on
    std::string diagnostics = "invalid profile expected one of: [";
    if (rv::supportedBundles() == std::set{fhirBundle::v_2022_01_01})
    {
        diagnostics += deprecatedProfs + "]";
    }
    else if (rv::supportedBundles() == std::set{fhirBundle::v_2022_01_01, fhirBundle::v_2023_07_01})
    {
        diagnostics += deprecatedProfs + ", " + newProfs + "]";

    }
    else
    {
        diagnostics += newProfs + "]";
    }

    auto body = ResourceManager::instance().getStringResource("test/EndpointHandlerTest/ERP-13187-POST_Communication_InvalidProfile.json");
    ASSERT_NO_FATAL_FAILURE(test(std::move(body), {.expectedResult = HttpStatus::BadRequest, .message = message, .diagnostics = diagnostics}));
}


struct CommunicationPostMixedProfileTestParams {
    model::ResourceVersion::DeGematikErezeptWorkflowR4 communicationVersion{};
    model::ResourceVersion::KbvItaErp medicationVersion;
};

class CommunicationPostMixedProfileTest : testutils::OverlappingFhirProfileEnvironmentGuard,
                                          public CommunicationPostEndpointTest,
                                          public ::testing::WithParamInterface<CommunicationPostMixedProfileTestParams>
{
public:
    static std::string name(const ::testing::TestParamInfo<CommunicationPostMixedProfileTestParams>& info)
    {
        std::ostringstream out;
        out << info.index << "_comm_" << v_str(info.param.communicationVersion) << "_medication_"
            << v_str(info.param.medicationVersion);
        return std::regex_replace(std::move(out).str(), std::regex{"[^A-Za-z0-9]"}, "_");
    }
};


TEST_P(CommunicationPostMixedProfileTest, good)
{
    static const ResourceTemplates::MedicationOptions medicationOptions{.version = GetParam().medicationVersion};
    auto body =
        CommunicationJsonStringBuilder{model::Communication::MessageType::InfoReq, GetParam().communicationVersion}
            .setPrescriptionId(
                model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                    .toString())
            .setSender(ActorRole::Insurant, kvnr)
            .setRecipient(ActorRole::Doctor, "A-Doctor")
            .setAbout("#" + medicationOptions.id)
            .setPayload("Haben sie das da?")
            .setMedicationOptions(medicationOptions)
            .createJsonString();
    ASSERT_NO_FATAL_FAILURE(test(body));
}


TEST_P(CommunicationPostMixedProfileTest, invalidMedication)
{
    static const ResourceTemplates::MedicationOptions medicationOptions{
        .version = GetParam().medicationVersion,
        .templatePrefix = "test/EndpointHandlerTest/invalid_medication_template_"};
    auto body =
        CommunicationJsonStringBuilder{model::Communication::MessageType::InfoReq, GetParam().communicationVersion}
            .setPrescriptionId(
                model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                    .toString())
            .setSender(ActorRole::Insurant, kvnr)
            .setRecipient(ActorRole::Doctor, "A-Doctor")
            .setAbout("#" + medicationOptions.id)
            .setPayload("Haben sie das da?")
            .setMedicationOptions(medicationOptions)
            .createJsonString();
    ASSERT_NO_FATAL_FAILURE(test(body, {HttpStatus::BadRequest}));
}

// clang-format off
namespace rv = model::ResourceVersion;
INSTANTIATE_TEST_SUITE_P(combinations, CommunicationPostMixedProfileTest,
        ::testing::ValuesIn<std::initializer_list<CommunicationPostMixedProfileTestParams>>({
        { rv::DeGematikErezeptWorkflowR4::v1_1_1, rv::KbvItaErp::v1_0_2 },
        { rv::DeGematikErezeptWorkflowR4::v1_1_1, rv::KbvItaErp::v1_1_0 },
        { rv::DeGematikErezeptWorkflowR4::v1_2_0, rv::KbvItaErp::v1_0_2 },
        { rv::DeGematikErezeptWorkflowR4::v1_2_0, rv::KbvItaErp::v1_1_0 },
}), &CommunicationPostMixedProfileTest::name);

// clang-format on
