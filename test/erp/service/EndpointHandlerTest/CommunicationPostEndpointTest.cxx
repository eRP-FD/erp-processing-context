/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/TestUtils.hxx"

#include <date/date.h>


struct CommunicationPostEndpointTestOptions {
    static inline const std::string kvnr{"X123456788"};
    HttpStatus expectedResult{HttpStatus::Created};
    std::optional<std::string> message = std::nullopt;
    std::optional<std::string> diagnostics = std::nullopt;
    JWT accessToken = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
};

class CommunicationPostEndpointTest : public EndpointHandlerTest
{
protected:
    static constexpr auto& kvnr = CommunicationPostEndpointTestOptions::kvnr;
    static inline const std::string kvnr2{"X123456789"};
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
        serverRequest.setAccessToken(opt.accessToken);
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
    auto body = CommunicationJsonStringBuilder{model::Communication::MessageType::DispReq}
                    .setPrescriptionId(model::PrescriptionId::fromDatabaseId(
                                           model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                                           .toString())
                    .setSender(ActorRole::Insurant, kvnr)
                    .setRecipient(ActorRole::Doctor, "A-Doctor")
                    .setAccessCode("8ec23b29b2b53d0f8ffd6a17275a584e753e5af4c5fdfc4c7a2f07f7383b2e60")
                    .setPayload(R"({"version":1, "supplyOptionsType": "onPremise"})")
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
    const auto& fhirInstance = Fhir::instance();
    const auto& backend = fhirInstance.backend();
    const auto supported =
        fhirInstance.structureRepository(model::Timestamp::now())
            .supportedVersions(&backend,
                               {
                                   // std::string{model::resource::structure_definition::communicationInfoReq},
                                   std::string{model::resource::structure_definition::communicationReply},
                                   std::string{model::resource::structure_definition::communicationDispReq},
                                   std::string{model::resource::structure_definition::communicationRepresentative},
                                   std::string{model::resource::structure_definition::communicationChargChangeReq},
                                   std::string{model::resource::structure_definition::communicationChargChangeReply},
                               });
    const auto& newProfs = String::join(supported, ", ");
    char message[] = "Invalid request body";
    std::string diagnostics =
        "invalid profile https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_ChargChangeReply|1.0 "
        "must be one of: " +
        newProfs;

    auto body = ResourceManager::instance().getStringResource(
        "test/EndpointHandlerTest/ERP-13187-POST_Communication_InvalidProfile.json");
    ASSERT_NO_FATAL_FAILURE(test(
        std::move(body), {.expectedResult = HttpStatus::BadRequest, .message = message, .diagnostics = diagnostics}));
}

struct CommunicationPostEndpointTestParams {
    enum Result {
        accept,
        reject,
    };
    Result result;
    std::string date;
    model::Communication::MessageType messageType;
    fhirtools::FhirVersion profileVersion;
};

std::ostream& operator << (std::ostream& out, const CommunicationPostEndpointTestParams& params)
{
    out << R"({ "result": ")" << magic_enum::enum_name(params.result) << R"(", )";
    out << R"("date": ")" << params.date << R"(", )";
    out << R"("messageType": ")" << magic_enum::enum_name(params.messageType) << R"(", )";
    out << R"("profileVersion": ")" << to_string(params.profileVersion) << R"(" })";
    return out;
}

class CommunicationPostEndpointTestP : public CommunicationPostEndpointTest,
                                       public testing::WithParamInterface<CommunicationPostEndpointTestParams>
{
public:
    static std::list<CommunicationPostEndpointTestParams> parameters()
    {
        using enum model::Communication::MessageType;
        using enum CommunicationPostEndpointTestParams::Result;
        return {
        // NOTE: as ERP-27522 is not actually implemented we are testing the original behavior.
            {accept, "2025-04-16", ChargChangeReq, ResourceTemplates::Versions::GEM_ERPCHRG_1_0},
            {accept, "2025-04-16", ChargChangeReply, ResourceTemplates::Versions::GEM_ERPCHRG_1_0},
            {reject, "2025-04-16", Representative, ResourceTemplates::Versions::GEM_ERP_1_2},
            {reject, "2025-04-16", DispReq, ResourceTemplates::Versions::GEM_ERP_1_2},
            {reject, "2025-04-16", Reply, ResourceTemplates::Versions::GEM_ERP_1_2},
            {reject, "2025-04-16", Representative, ResourceTemplates::Versions::GEM_ERP_1_3},
            {reject, "2025-04-16", DispReq, ResourceTemplates::Versions::GEM_ERP_1_3},
            {reject, "2025-04-16", Reply, ResourceTemplates::Versions::GEM_ERP_1_3},
            {accept, "2025-04-16", Representative, ResourceTemplates::Versions::GEM_ERP_1_4},
            {accept, "2025-04-16", DispReq, ResourceTemplates::Versions::GEM_ERP_1_4},
            {accept, "2025-04-16", Reply, ResourceTemplates::Versions::GEM_ERP_1_4},
        };
    }

};

TEST_P(CommunicationPostEndpointTestP, ProfileValidity)
{
    auto today{date::floor<date::days>(std::chrono::system_clock::now())};
    date::sys_days testDate;
    std::istringstream datestrm{GetParam().date};
    date::from_stream(datestrm, "%Y-%m-%d", testDate);
    testutils::ShiftFhirResourceViewsGuard offsetGuard{today - testDate};
    CommunicationJsonStringBuilder builder{GetParam().messageType, GetParam().profileVersion};
    builder.setPayload(R"({"version":1, "supplyOptionsType": "onPremise"})");
    CommunicationPostEndpointTestOptions opts;
    auto gkvPrescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto pkvPrescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    switch (GetParam().messageType)
    {
        using enum model::Communication::MessageType;
        case InfoReq:
            Fail("InfoReq not supported.");
        case Representative:
            builder.setSender(ActorRole::Insurant, kvnr).setRecipient(ActorRole::Insurant, kvnr2);
            builder.setPrescriptionId(gkvPrescriptionId.toString());
            builder.setAccessCode("8ec23b29b2b53d0f8ffd6a17275a584e753e5af4c5fdfc4c7a2f07f7383b2e60");
            break;
        case ChargChangeReq:
            builder.setSender(ActorRole::PkvInsurant, kvnr).setRecipient(ActorRole::Pharmacists, "A-Pharmacy");
            builder.setPrescriptionId(pkvPrescriptionId.toString());
            break;
        case DispReq:
            builder.setSender(ActorRole::Insurant, kvnr).setRecipient(ActorRole::Pharmacists, "A-Pharmacy");
            builder.setPrescriptionId(gkvPrescriptionId.toString());
            builder.setAccessCode("8ec23b29b2b53d0f8ffd6a17275a584e753e5af4c5fdfc4c7a2f07f7383b2e60");
            break;
        case ChargChangeReply:
            builder.setSender(ActorRole::Pharmacists, "A-Pharmacy").setRecipient(ActorRole::PkvInsurant, kvnr);
            builder.setPrescriptionId(pkvPrescriptionId.toString());
            opts.accessToken = JwtBuilder::testBuilder().makeJwtApotheke("A-Pharmacy");
            break;
        case Reply:
            builder.setSender(ActorRole::Pharmacists, "A-Pharmacy").setRecipient(ActorRole::Insurant, kvnr);
            builder.setPrescriptionId(gkvPrescriptionId.toString());
            builder.setAccessCode("8ec23b29b2b53d0f8ffd6a17275a584e753e5af4c5fdfc4c7a2f07f7383b2e60");
            opts.accessToken = JwtBuilder::testBuilder().makeJwtApotheke("A-Pharmacy");
    }
    switch (GetParam().result)
    {
        case CommunicationPostEndpointTestParams::accept:
            opts.expectedResult = HttpStatus::Created;
            break;
        case CommunicationPostEndpointTestParams::reject:
            opts.expectedResult = HttpStatus::BadRequest;
            break;
    }
    ASSERT_NO_FATAL_FAILURE(test(builder.createJsonString(), opts));
}

INSTANTIATE_TEST_SUITE_P(parameters, CommunicationPostEndpointTestP,
                         testing::ValuesIn(CommunicationPostEndpointTestP::parameters()));
