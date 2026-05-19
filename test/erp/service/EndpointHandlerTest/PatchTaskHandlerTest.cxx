/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#include "erp/model/eu/GemErpEuPrTaskInput.hxx"
#include "erp/model/extensions/GemErpRedeemableByPatient.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/eu/PatchTaskHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"
#include "test/mock/MockDatabase.hxx"
#include "erp/database/DatabaseFrontend.hxx"

class PatchTaskHandlerTest : public EndpointHandlerTest
{
public:
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Parameters xmlns="http://hl7.org/fhir">
  <meta>
    <profile value="https://gematik.de/fhir/erp-eu/StructureDefinition/GEM_ERPEU_PR_PAR_PATCH_Task_Input|1.1"/>
  </meta>
  <parameter>
    <name value="eu-isRedeemableByPatientAuthorization"/>
    <valueBoolean value="true"/>
  </parameter>
</Parameters>)";
    const JWT jwtInsurant0 = JwtBuilder::testBuilder().makeJwtVersicherter("X234567892");
    const JWT jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter("X234567891");
    const model::PrescriptionId taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714);
    const model::PrescriptionId taskIdDirekt = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4711);
    const std::string taskJson = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    const std::string kbvBundle = ResourceTemplates::kbvBundleXml();
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";


    ServerRequest request(const model::PrescriptionId& id, const JWT& jwt)
    {
        const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
        Header requestHeader{HttpMethod::PATCH,
                             "/Task/" + id.toString(),
                             0,
                             {{Header::ContentType, ContentMimeType::xml}},
                             HttpStatus::Unknown};

        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id.toString()});
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        serverRequest.setAccessToken(jwt);
        serverRequest.setBody(isEuRedeemableByPatientAuthorizationParam.serializeToXmlString());
        return serverRequest;
    }

};

TEST_F(PatchTaskHandlerTest, MissingTaskId_Throws_Success)
{
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");
    eu::PatchTaskHandler patchTaskHandler({});
    Header requestHeader{HttpMethod::PATCH,
                         "/Task/" + taskId.toString(),
                         0,
                         { {Header::ContentType, ContentMimeType::xml} },
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    ASSERT_THROW(patchTaskHandler.handleRequest(sessionContext), ErpException);
}

TEST_F(PatchTaskHandlerTest, KvnrMismatch_Throws_Success)
{
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");
    const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
    eu::PatchTaskHandler patchTaskHandler({});
    Header requestHeader{HttpMethod::PATCH,
                         "/Task/" + taskId.toString(),
                         0,
                         { {Header::ContentType, ContentMimeType::xml} },
                         HttpStatus::Unknown};

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {taskId.toString()});
    serverRequest.setQueryParameters({{"ac", validAccessCode}});
    serverRequest.setAccessToken(jwtInsurant0);
    serverRequest.setBody(isEuRedeemableByPatientAuthorizationParam.serializeToXmlString());

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION(patchTaskHandler.handleRequest(sessionContext), HttpStatus::Forbidden);
}

TEST_F(PatchTaskHandlerTest, RedeemPrescriptionByUser_NonRedeemableByProperties_Fail)
{
    A_28500.test("Task markieren - Versicherter - nur einlösbare E-Rezepte");
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");

    const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
    eu::PatchTaskHandler patchTaskHandler({});
    Header requestHeader{HttpMethod::PATCH,
                         "/Task/" + taskIdDirekt.toString(),
                         0,
                         { {Header::ContentType, ContentMimeType::xml} },
                         HttpStatus::Unknown};

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {taskIdDirekt.toString()});
    serverRequest.setQueryParameters({{"ac", validAccessCode}});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter("X123456788"));
    serverRequest.setBody(isEuRedeemableByPatientAuthorizationParam.serializeToXmlString());

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    auto taskAndKey = sessionContext.database()->retrieveTaskForUpdate(taskIdDirekt);
    EXPECT_TRUE(taskAndKey.has_value());
    sessionContext.database()->activateTask(taskAndKey.value().task, model::Binary{Uuid().toString(), "HealthCareProviderPrescription"}, mJwtBuilder->makeJwtArzt());

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION(patchTaskHandler.handleRequest(sessionContext), HttpStatus::Conflict);
}

TEST_F(PatchTaskHandlerTest, RedeemPrescriptionByUser_RedeemableByPropertiesAfterActivate_Success)
{
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");

    const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
    eu::PatchTaskHandler patchTaskHandler({});
    ServerRequest serverRequest =  request(taskId, jwtInsurant);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    auto taskAndKey = sessionContext.database()->retrieveTaskForUpdate(taskId);
    EXPECT_TRUE(taskAndKey.has_value());
    sessionContext.database()->activateTask(taskAndKey.value().task, model::Binary{Uuid().toString(), "HealthCareProviderPrescription"}, mJwtBuilder->makeJwtArzt());

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(patchTaskHandler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    auto resultTask = model::Task::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator());
    EXPECT_TRUE(resultTask.isEuRedeemableByPatientAuthorization());
}


TEST_F(PatchTaskHandlerTest, noSuchTask)
{
    const auto deadID00 =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 0xDead'1D'00);
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");

    const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
    eu::PatchTaskHandler patchTaskHandler({});
    ServerRequest serverRequest = request(deadID00, jwtInsurant);

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(patchTaskHandler.handleRequest(sessionContext), HttpStatus::NotFound, "No such task.");
}


class PatchTaskHandlerStatusTest : public PatchTaskHandlerTest,
                                   public testing::WithParamInterface<model::PrescriptionId>
{
public:
    static inline const model::PrescriptionId taskIdDraft =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    static inline const model::PrescriptionId taskIdInProgres =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    static inline const model::PrescriptionId taskIdCompleted =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4719);
    static inline const model::PrescriptionId taskIdCancelled =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4712);

    void SetUp() override
    {
        const auto now = model::Timestamp::now();
        mockDatabase->updateTaskClearPersonalData(taskIdCancelled, model::Task::Status::cancelled, now, now);
        auto db = mServiceContext.databaseFactory();
        auto taskAndKey = db->retrieveTaskForUpdate(GetParam());
        db->commitTransaction();
        ASSERT_TRUE(taskAndKey.has_value());
        kvnr = taskAndKey->task.kvnr().value_or(model::Kvnr{"X234567891"}).id();
    }
    std::string kvnr;
};

TEST_P(PatchTaskHandlerStatusTest, taskStatus)
{
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");
    const auto isEuRedeemableByPatientAuthorizationParam = model::GemErpEuPrTaskInput::fromXmlNoValidation(xml);
    eu::PatchTaskHandler patchTaskHandler({});
    ServerResponse serverResponse;
    AccessLog accessLog;
    ServerRequest serverRequest = request(GetParam(), mJwtBuilder->makeJwtVersicherter(kvnr));
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(patchTaskHandler.handleRequest(sessionContext), HttpStatus::Forbidden,
                                      "Operation only allowed by insurant owning the task and task status is ready");
}


INSTANTIATE_TEST_SUITE_P(fail, PatchTaskHandlerStatusTest, testing::Values(
    PatchTaskHandlerStatusTest::taskIdDraft,
    PatchTaskHandlerStatusTest::taskIdInProgres,
    PatchTaskHandlerStatusTest::taskIdCompleted,
    PatchTaskHandlerStatusTest::taskIdCancelled
));
