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
    <profile value="https://gematik.de/fhir/erp-eu/StructureDefinition/GEM_ERPEU_PR_PAR_PATCH_Task_Input|1.0"/>
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

    testutils::ShiftFhirResourceViewsGuard shift{"EU_2025_10_01",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
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
    serverRequest.setAccessToken(jwtInsurant);
    serverRequest.setBody(isEuRedeemableByPatientAuthorizationParam.serializeToXmlString());

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(patchTaskHandler.preHandleRequestHook(sessionContext));
    ASSERT_THROW(patchTaskHandler.handleRequest(sessionContext), ErpException);
}

TEST_F(PatchTaskHandlerTest, RedeemPrescriptionByUser_RedeemableByPropertiesAfterActivate_Success)
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
    serverRequest.setAccessToken(jwtInsurant);
    serverRequest.setBody(isEuRedeemableByPatientAuthorizationParam.serializeToXmlString());

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
