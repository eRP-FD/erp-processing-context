/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/DeviceHandler.hxx"
#include "erp/service/MetaDataHandler.hxx"
#include "erp/service/task/AbortTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/RejectTaskHandler.hxx"
#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPostHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/service/consent/ConsentPostHandler.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/mock/RegistrationMock.hxx"

#include <gtest/gtest.h>
#include <variant>


/**
 * When creating an EndpointHandlerTest instance a PcServiceContext is created as a member of the
 * EndpointHandlerTest class (mServiceContext). Access to the database is controlled via the
 * ServiceContext hosting a database factory instance. For the EndpointHandlerTests the database
 * creation function is a lambda function creating an instance of class MockDatabase. This lambda
 * function also fills the MockDatabase with static test data.
 * The PcServiceContext also creates a PreUserPseudonymManager instance trying to load CMAC for
 * PreUserPseudonym from Database. This invokes "PcServiceContext.databaseFactory" and the
 * MockDatabase is created and filled with static data for the first time. As the MockDatabase
 * is only created temporarily in the "LoadCmacs" method the MockDatabase is immediately destroyed
 * afterwards.
 * The next time the MockDatabase is created and filled with static data either if the
 * EndpointHandlerTests or one of the EndpointHandlers are trying to access the database via the
 * HandlerContext and its service context.
 *
 * If the mock database is created it is also filled with static data.
 * This inserts the following task resources from json files located in the sub directory
 * "resources/test/EndpointHandlerTest" into the database:
 * 
 *              | Status      | KVNR       | Prescription-Id / AccessCode / Secret                            |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task1.json | ready       | X123456789 | 160.000.000.004.711.86                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            |                                                                  |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task2.json | ready       | X123456789 | 160.000.000.004.712.83                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task3.json | draft       |            | 160.000.000.004.713.18                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            |                                                                  |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task4.json | ready       | X234567890 | 160.000.000.004.714.77                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            |                                                                  |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task5.json | in-progress | X234567890 | 160.000.000.004.715.74                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            | 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task6.json | in-progress | X234567890 | 160.000.000.004.716.71                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            | 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task7.json | ready       | X234567890 | 160.000.000.004.717.68                                           |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            |                                                                  |
 * -------------+-------------+------------+------------------------------------------------------------------+
 *
 * Also available on disk are the following files containing json definitions for tasks:
 *
 *              | Status      | KVNR       | Prescription-Id / AccessCode / Secret                            |
 * -------------+-------------+------------+------------------------------------------------------------------+
 * - task8.json | ready       | X123456789 | 160.000.000.004.71.86                                            |
 *              |             |            | 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea |
 *              |             |            |                                                                  |
 * -------------+-------------+------------+------------------------------------------------------------------+
 *
*/

TEST_F(EndpointHandlerTest, GetTaskById)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    const auto idStr = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711).toString();

    Header requestHeader{ HttpMethod::GET, "/Task/" + idStr, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({ "id" }, { idStr });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog };


    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::string bodyActual;
    ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(bodyActual));

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));
    auto bundle = model::Bundle::fromJsonNoValidation(bodyActual);
    auto tasks = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));


    ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");
//    ASSERT_EQ(rapidjson::Pointer("/type").Get(document)->GetString(), "searchset");
    ASSERT_TRUE(rapidjson::Pointer("/link/0/relation").Get(document)->IsString());

    ASSERT_EQ(std::string(rapidjson::Pointer("/entry/0/resource/resourceType").Get(document)->GetString()), "Task");

    serverRequest.setPathParameters({ "id" }, { "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0" });
    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
}

TEST_F(EndpointHandlerTest, GetTaskById169NoAccessCode)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    const auto idStr = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4711).toString();

    Header requestHeader{ HttpMethod::GET, "/Task/" + idStr, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({ "id" }, { idStr });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog };


    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::string bodyActual;
    ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(bodyActual));

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));
    auto bundle = model::Bundle::fromJsonNoValidation(bodyActual);
    auto tasks = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);

    ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));

    ASSERT_THROW((void)tasks[0].accessCode(), model::ModelException);

}

TEST_F(EndpointHandlerTest, GetTaskByIdPatientConfirmation)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    const auto idStr = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();

    Header requestHeader{ HttpMethod::GET, "/Task/" + idStr, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X234567890"));
    serverRequest.setPathParameters({ "id" }, { idStr });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};


    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::string bodyActual;
    ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(bodyActual));

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));
    auto bundle = model::Bundle::fromJsonNoValidation(bodyActual);

    auto prescriptions = bundle.getResourcesByType<model::Bundle>("Bundle");
    ASSERT_EQ(prescriptions.size(), 1);

    ASSERT_NO_THROW(model::KbvBundle::fromXml(prescriptions[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                              *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle));

    ASSERT_TRUE(prescriptions[0].getSignature().has_value());
    auto signature = prescriptions[0].getSignature().value();
    ASSERT_TRUE(signature.when().has_value());
    ASSERT_TRUE(signature.who().has_value());
    EXPECT_EQ(UrlHelper::parseUrl(std::string(signature.who().value())).mPath,
              "/Device/" + std::to_string(model::Device::Id));
    ASSERT_TRUE(signature.sigFormat().has_value());
    ASSERT_EQ(std::string(signature.sigFormat().value()), std::string(MimeType::jose));
    ASSERT_TRUE(signature.targetFormat().has_value());
    ASSERT_EQ(std::string(signature.targetFormat().value()), std::string(MimeType::fhirJson));
    ASSERT_TRUE(signature.data().has_value());

    auto jsonSignature=model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(signature.jsonDocument());

    rapidjson::Pointer systemPointer("/type/0/system");
    ASSERT_TRUE(systemPointer.Get(jsonSignature));
    EXPECT_EQ(std::string(systemPointer.Get(jsonSignature)->GetString()), "urn:iso-astm:E1762-95:2013");
    rapidjson::Pointer codePointer("/type/0/code");
    ASSERT_TRUE(codePointer.Get(jsonSignature));
    EXPECT_EQ(std::string(codePointer.Get(jsonSignature)->GetString()), "1.2.840.10065.1.12.1.5");

    auto signatureSerialized = Base64::decodeToString(*signature.data());
    auto parts = String::split(signatureSerialized, '.');
    ASSERT_EQ(parts.size(), 3);
    ASSERT_TRUE(parts[1].empty());
}

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingKVNR)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116.test("Get Task using the correct KVNR");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456789
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{ HttpMethod::GET, "/Task/160.000.000.004.711.86", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    const auto responseBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(responseBundle.jsonDocument()),
        SchemaType::fhir));
    const auto tasks = responseBundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), "160.000.000.004.711.86");
    ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
}

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeUrl)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116.test("Get Task wrong KVNR and using the correct AccessCode passed by URL");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456789
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{ HttpMethod::GET, "/Task/160.000.000.004.711.86", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654321"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });
    serverRequest.setQueryParameters({{{ "ac" }, { "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea" }}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    const auto responseBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(responseBundle.jsonDocument()),
        SchemaType::fhir));
    const auto tasks = responseBundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), "160.000.000.004.711.86");
    ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
}

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeHeader)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116.test("Get Task wrong KVNR and using the correct AccessCode passed by Http-Header");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456789
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{
        HttpMethod::GET,
        "/Task/160.000.000.004.711.86",
        0,
        {{Header::XAccessCode, "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea"}},
        HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654321"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    const auto responseBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(responseBundle.jsonDocument()),
        SchemaType::fhir));
    const auto tasks = responseBundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), "160.000.000.004.711.86");
    ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
}

TEST_F(EndpointHandlerTest, GetTaskByIdWrongAccessCodeHeader)
{
    A_19116.test("Get Task wrong KVNR and using the wrong AccessCode");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456789
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{HttpMethod::GET,
                         "/Task/160.000.000.004.711.86",
                         0,
                         {{Header::XAccessCode, "wrong_access_code"}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654321"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
    ASSERT_TRUE(serverResponse.getBody().empty());
}

TEST_F(EndpointHandlerTest, GetTaskByIdMissingAccessCode)
{
    A_19116.test("Get Task using wrong KVNR and without AccessCode");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456789
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{HttpMethod::GET,
                         "/Task/160.000.000.004.711.86",
                         0,
                         {},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654321"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
    ASSERT_TRUE(serverResponse.getBody().empty());
}

TEST_F(EndpointHandlerTest, GetTaskById_A20753_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    A_20753.test("Exclusion of representative communication on or by means of verification identity");
    {
        Header requestHeader{ HttpMethod::GET, {}, Header::Version_1_1, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{ std::move(requestHeader) };
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        // Format of KVNRs of test cards:
        // "X0000nnnnP" with 1 <= nnnn <= 5000; P = Verification Digit
        std::string kvnrInsurant = "X123456789";
        std::string kvnrTestCard = "X000022227";
        std::string accessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
        model::Task taskInsurant = addTaskToDatabase(
            sessionContext, model::Task::Status::ready, accessCode, kvnrInsurant);
        model::Task taskTestCard = addTaskToDatabase(
            sessionContext, model::Task::Status::ready, accessCode, kvnrTestCard);

        // Allowed access: Insurant accesses task of insurant
        sessionContext.request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnrInsurant));
        sessionContext.request.setPathParameters({ "id" }, { taskInsurant.prescriptionId().toString() });
        sessionContext.request.setQueryParameters({ {{ "ac" }, { accessCode }} });
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        // Allowed access: KVNR of test card accesses task of test card
        sessionContext.request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnrTestCard));
        sessionContext.request.setPathParameters({ "id" }, { taskTestCard.prescriptionId().toString() });
        sessionContext.request.setQueryParameters({ {{ "ac" }, { accessCode }} });
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        // Not allowed access: Insurant accesses task of test card
        sessionContext.request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnrInsurant));
        sessionContext.request.setPathParameters({ "id" }, { taskTestCard.prescriptionId().toString() });
        sessionContext.request.setQueryParameters({ {{ "ac" }, { accessCode }} });
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);

        // Not allowed access: KVNR of test card accesses task of insurant
        sessionContext.request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnrTestCard));
        sessionContext.request.setPathParameters({ "id" }, { taskInsurant.prescriptionId().toString() });
        sessionContext.request.setQueryParameters({ {{ "ac" }, { accessCode }} });
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
}

TEST_F(EndpointHandlerTest, GetAllTasks)//NOLINT(readability-function-cognitive-complexity)
{
    GetAllTasksHandler handler({});

    rapidjson::Document jwtDocument;
    std::string jwtClaims = R"({
             "professionOID": "1.2.276.0.76.4.49",
             "sub":           "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk",
             "idNummer":      "X123456789"
        })";

    jwtDocument.Parse(jwtClaims);
    auto jwt = JwtBuilder(MockCryptography::getIdpPrivateKey()).getJWT(jwtDocument);

    Header requestHeader{ HttpMethod::GET, "/Task/", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::string bodyActual;
    ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(bodyActual));
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));

    ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");
//    ASSERT_EQ(rapidjson::Pointer("/type").Get(document)->GetString(), "searchset");
    ASSERT_TRUE(rapidjson::Pointer("/link/0/relation").Get(document)->IsString());

    const auto* entry0resourceType = rapidjson::Pointer("/entry/0/resource/resourceType").Get(document);
    ASSERT_TRUE(entry0resourceType);
    ASSERT_TRUE(entry0resourceType->IsString());
    ASSERT_EQ(std::string(entry0resourceType->GetString()), "Task");

    auto bundle =
        model::Bundle::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(document));
    auto tasks = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_FALSE(tasks.empty());
    for (const auto& item : tasks)
    {
        ASSERT_NO_THROW(model::Task::fromXml(item.serializeToXmlString(), *StaticData::getXmlValidator(),
                                             *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
    }
}

// Regression Test for bugticket ERP-6560 (Task bundle doesn't contain paging links to next and prev)
TEST_F(EndpointHandlerTest, GetAllTasksErp6560)//NOLINT(readability-function-cognitive-complexity)
{
    GetAllTasksHandler handler({});

    rapidjson::Document jwtDocument;
    std::string jwtClaims = R"({
             "professionOID": "1.2.276.0.76.4.49",
             "sub":           "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk",
             "idNummer":      "X123456789"
    })";

    jwtDocument.Parse(jwtClaims);
    auto jwt = JwtBuilder(MockCryptography::getIdpPrivateKey()).getJWT(jwtDocument);


    // 3 Task are expected:
    {
        Header requestHeader{ HttpMethod::GET, "/Task/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{ std::move(requestHeader) };
        serverRequest.setAccessToken(JWT(jwt));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        std::string bodyActual;
        ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

        rapidjson::Document document;
        ASSERT_NO_THROW(document.Parse(bodyActual));
        ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));

        ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");

        auto bundle =
            model::Bundle::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(document));
        ASSERT_EQ(bundle.getResourceCount(), 3);
        ASSERT_TRUE(bundle.getLink(model::Link::Self).has_value());
        ASSERT_FALSE(bundle.getLink(model::Link::Next).has_value());
        //ASSERT_EQ(std::string(bundle.getLink(model::Link::Next).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=50");
        ASSERT_FALSE(bundle.getLink(model::Link::Prev).has_value());
    }


    // Fill in 60 Tasks, page 1 with 50 entries expected
    {
        Header requestHeader{ HttpMethod::GET, "/Task/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{ std::move(requestHeader) };
        serverRequest.setAccessToken(JWT(jwt));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        for (int i = 0; i < 110; ++i)
        {
            addTaskToDatabase(sessionContext, model::Task::Status::ready, "access-code", "X123456789");
        }

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        std::string bodyActual;
        ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

        rapidjson::Document document;
        ASSERT_NO_THROW(document.Parse(bodyActual));
        ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));

        ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");

        auto bundle =
            model::Bundle::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(document));
        ASSERT_EQ(bundle.getResourceCount(), 50);
        ASSERT_TRUE(bundle.getLink(model::Link::Self).has_value());
        ASSERT_TRUE(bundle.getLink(model::Link::Next).has_value());
        std::string val(bundle.getLink(model::Link::Next).value());
        ASSERT_EQ(val, "https://gematik.erppre.de:443/Task?_count=50&__offset=50");
        ASSERT_FALSE(bundle.getLink(model::Link::Prev).has_value());


        // Page 2 with 50 entries expected
        sessionContext.request.setQueryParameters({{"_count", "50"}, {"__offset", "50"}});

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

        ASSERT_NO_THROW(document.Parse(bodyActual));
        ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));

        ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");

        auto bundle2 =
            model::Bundle::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(document));
        ASSERT_EQ(bundle2.getResourceCount(), 50);
        ASSERT_TRUE(bundle2.getLink(model::Link::Self).has_value());
        ASSERT_TRUE(bundle2.getLink(model::Link::Next).has_value());
        ASSERT_EQ(std::string(bundle2.getLink(model::Link::Next).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=100");
        ASSERT_TRUE(bundle2.getLink(model::Link::Prev).has_value());
        ASSERT_EQ(std::string(bundle2.getLink(model::Link::Prev).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=0");


        // Page 3 with 12 entries expected
        sessionContext.request.setQueryParameters({{"_count", "50"}, {"__offset", "100"}});

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        ASSERT_NO_FATAL_FAILURE(bodyActual = canonicalJson(serverResponse.getBody()));

        ASSERT_NO_THROW(document.Parse(bodyActual));
        ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(document, SchemaType::fhir));

        ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");

        auto bundle3 =
            model::Bundle::fromJson(model::NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(document));
        ASSERT_EQ(bundle3.getResourceCount(), 13);
        ASSERT_TRUE(bundle3.getLink(model::Link::Self).has_value());
        ASSERT_FALSE(bundle3.getLink(model::Link::Next).has_value());
        //ASSERT_EQ(std::string(bundle3.getLink(model::Link::Next).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=150");
        ASSERT_TRUE(bundle3.getLink(model::Link::Prev).has_value());
        ASSERT_EQ(std::string(bundle3.getLink(model::Link::Prev).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=50");
    }
}

TEST_F(EndpointHandlerTest, CreateTask)//NOLINT(readability-function-cognitive-complexity)
{
    CreateTaskHandler handler({});

    Header requestHeader{ HttpMethod::POST, "/Task/$create", 0, {{Header::ContentType,ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };

    std::string parameters = R"--(<Parameters xmlns="http://hl7.org/fhir">
  <parameter>
    <name value="workflowType"/>
    <valueCoding>
      <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
      <code value="160"/>
    </valueCoding>
  </parameter>
</Parameters>)--";

    serverRequest.setBody(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);

    std::optional<model::Task> retrievedTask;
    ASSERT_NO_THROW(retrievedTask.emplace(model::Task::fromXml(serverResponse.getBody(),
                                                               *StaticData::getXmlValidator(),
                                                               *StaticData::getInCodeValidator(),
                                                               SchemaType::Gem_erxTask)));
    EXPECT_GT(retrievedTask->prescriptionId().toDatabaseId(), 0);
    EXPECT_EQ(retrievedTask->status(), model::Task::Status::draft);
    EXPECT_FALSE(retrievedTask->kvnr().has_value());
    EXPECT_EQ(retrievedTask->type(), model::PrescriptionType::apothekenpflichigeArzneimittel);
    const auto ac = retrievedTask->accessCode();
    EXPECT_EQ(ac.size(), 64);
}

TEST_F(EndpointHandlerTest, CreateTaskEmptyBodyJson)
{
    CreateTaskHandler handler({});

    Header requestHeader{ HttpMethod::POST, "/Task/$create", 0, {{Header::ContentType,ContentMimeType::fhirJsonUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };

    std::string parameters;

    serverRequest.setBody(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
}

TEST_F(EndpointHandlerTest, CreateTaskEmptyBodyXml)
{
    CreateTaskHandler handler({});

    Header requestHeader{ HttpMethod::POST, "/Task/$create", 0, {{Header::ContentType,ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };

    std::string parameters;

    serverRequest.setBody(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
}






TEST_F(EndpointHandlerTest, GetAllAuditEvents)
{
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446688", "audit_event.json"));
}

TEST_F(EndpointHandlerTest, GetAllAuditEvents_delete_task)
{
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446689", "audit_event_delete_task.json"));
}

TEST_F(EndpointHandlerTest, GetAuditEvent)//NOLINT(readability-function-cognitive-complexity)
{
    GetAuditEventHandler handler({});

    const std::string id = "01eb69a4-9029-d610-b1cf-ddb8c6c0bfbc";
    Header requestHeader{ HttpMethod::GET, "/AuditEvent/" + id, 0, {}, HttpStatus::Unknown};

    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X122446688");
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setAccessToken(std::move(jwt));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    auto auditEvent = model::AuditEvent::fromJsonNoValidation(serverResponse.getBody());

    ASSERT_NO_THROW(model::Task::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxAuditEvent));

    auto expectedAuditEvent =
        model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/audit_event.json"));

    ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
              canonicalJson(expectedAuditEvent.serializeToJsonString()));
}

namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkAcceptTaskSuccessCommon(
    std::optional<model::Bundle>& resultBundle,
    PcServiceContext& serviceContext,
    const std::string_view& taskJson,
    const std::string_view& kbvBundleXml,
    unsigned int numOfExpectedResources)
{
    const auto& task = model::Task::fromJsonNoValidation(taskJson);

    AcceptTaskHandler handler({});
    Header requestHeader{ HttpMethod::POST, "/Task/" + task.prescriptionId().toString() + "/$accept/", 0, {}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { task.prescriptionId().toString() });
    serverRequest.setQueryParameters({ { "ac", std::string(task.accessCode()) } });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    ASSERT_NO_THROW(resultBundle = model::Bundle::fromXml(serverResponse.getBody(),
                                                          *StaticData::getXmlValidator(),
                                                          *StaticData::getInCodeValidator(),
                                                          SchemaType::fhir));
    ASSERT_EQ(resultBundle->getResourceCount(), numOfExpectedResources);

    const auto tasks = resultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    EXPECT_EQ(tasks[0].prescriptionId(), task.prescriptionId());
    EXPECT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    EXPECT_TRUE(tasks[0].secret().has_value());

    const auto binaryResources = resultBundle->getResourcesByType<model::Binary>("Binary");
    ASSERT_EQ(binaryResources.size(), 1);
    ASSERT_TRUE(binaryResources[0].data().has_value());
    std::optional<CadesBesSignature> signature;
    ASSERT_NO_THROW(signature.emplace(std::string{binaryResources[0].data().value()}));
    EXPECT_EQ(signature->payload(), kbvBundleXml);

    ASSERT_TRUE(tasks[0].healthCarePrescriptionUuid().has_value());
    ASSERT_TRUE(binaryResources[0].id().has_value());
    ASSERT_EQ(tasks[0].healthCarePrescriptionUuid().value(), binaryResources[0].id().value());
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, AcceptTaskSuccess)
{
    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        resourceManager.getStringResource(dataPath + "/task4.json"),
        resourceManager.getStringResource(dataPath + "/kbv_bundle.xml"),
        2 /*numOfExpectedResources*/));
}

TEST_F(EndpointHandlerTest, AcceptTaskPkvWithConsent)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const char* const pkvKvnr = "X500000000";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"), pkvTaskId.toString()), pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"), pkvTaskId.toString()), pkvKvnr),
        3 /*numOfExpectedResources*/));

    // Check consent:
    const auto consents = resultBundle->getResourcesByType<model::Consent>("Consent");
    ASSERT_EQ(consents.size(), 1);
    ASSERT_TRUE(consents[0].isChargingConsent());
    const auto tasks = resultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_TRUE(tasks[0].kvnr().has_value());
    ASSERT_EQ(tasks[0].kvnr().value(), consents[0].patientKvnr());
}

TEST_F(EndpointHandlerTest, AcceptTaskPkvWithoutConsent)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50001);
    const char* const pkvKvnr = "X500000001";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"), pkvTaskId.toString()), pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"), pkvTaskId.toString()), pkvKvnr),
        2 /*numOfExpectedResources*/));  // no consent resource in result;
}

TEST_F(EndpointHandlerTest, AcceptTaskFail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    AcceptTaskHandler handler({});
    {
        // Unknown Task;
        const auto id = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichigeArzneimittel, 999999).toString();
        Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{ std::move(requestHeader) };
        serverRequest.setPathParameters({ "id" }, { id });
        serverRequest.setQueryParameters({ { "ac", validAccessCode} });
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);

        serverRequest.setPathParameters({ "id" }, { "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0" });
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
    }

    const auto id = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    const Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
    {
        // No Access Code;
        ServerRequest serverRequest{ Header(requestHeader) };
        serverRequest.setPathParameters({ "id" }, { id });
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
    {
        // Invalid Access Code;
        ServerRequest serverRequest{ Header(requestHeader) };
        serverRequest.setPathParameters({ "id" }, { id });
        serverRequest.setQueryParameters({ { "ac", "invalid_access_code"} });
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
    {
        // No id;
        ServerRequest serverRequest{ Header(requestHeader) };
        serverRequest.setQueryParameters({ { "ac", validAccessCode} });
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
}

namespace
{

using QueryParameters = std::vector<std::pair<std::string,std::string>>;

std::string getId(const std::variant<int64_t, std::string>& databaseId,
                  model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel)
{
    try
    {
        return model::PrescriptionId::fromDatabaseId(
                   type,
                   std::get<int64_t>(databaseId))
            .toString();
    }
    catch (const std::exception&)
    {
        return std::get<std::string>(databaseId);
    }
}

template<class HandlerType>
void callHandlerWithResponseStatusCheck(
    PcSessionContext& sessionContext,
    HandlerType& handler,
    const HttpStatus expectedStatus)
{
    auto status = HttpStatus::Unknown;
    try
    {
        handler.preHandleRequestHook(sessionContext);
        handler.handleRequest(sessionContext);
        status = sessionContext.response.getHeader().status();
    }
    catch(const ErpException& exc)
    {
        status = exc.status();
    }
    catch(std::exception& exc)
    {
        FAIL() << "Unexpected exception " << exc.what();
    }
    ASSERT_EQ(status, expectedStatus);
}

template<class HandlerType>
void checkTaskOperation(
    const std::string& operationName,
    PcServiceContext& serviceContext,
    JWT jwt,
    const std::variant<int64_t, std::string>& taskId,
    Header::keyValueMap_t&& headers,
    QueryParameters&& queryParameters,
    const HttpStatus expectedStatus,
    model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel)
{
    HandlerType handler({});
    const auto id = getId(taskId, type);
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/" + operationName + "/", 0, std::move(headers), HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setQueryParameters(std::move(queryParameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, AbortTask)//NOLINT(readability-function-cognitive-complexity)
{
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$abort";
    // not found
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtPharmacy, 999999999, { }, { }, HttpStatus::NotFound));
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtPharmacy, "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0",
        { }, { }, HttpStatus::NotFound));

    // Task "in-progress":
    const auto taskInProgressId = 4716;

    // Pharmacy, no secret:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtPharmacy, taskInProgressId, { }, { }, HttpStatus::Forbidden));
    // Pharmacy, valid secret:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtPharmacy, taskInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::NoContent));

    const std::string kvnr = "X234567890";
    // Insurant -> invalid status:
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtInsurant1, taskInProgressId, { }, { }, HttpStatus::Forbidden));

    // Doctor -> invalid status:
    const auto jwtDoctor = JwtBuilder::testBuilder().makeJwtArzt();
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtDoctor, taskInProgressId, { }, { }, HttpStatus::Forbidden));

    // Task not "in-progress":
    const auto taskNotInProgressId = 4717;

    // Insurant without AccessCode header -> kvnr check (valid):
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtInsurant1, taskNotInProgressId, { }, { }, HttpStatus::NoContent));
    // Insurant without AccessCode header -> kvnr check (invalid):
    const auto jwtInsurant2 = JwtBuilder::testBuilder().makeJwtVersicherter("Unknown");
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtInsurant2, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));

    const std::string accessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    // Insurant with AccessCode header (valid)
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtInsurant2, taskNotInProgressId,
        { {"X-AccessCode", accessCode} }, { }, HttpStatus::NoContent));
    // Insurant with AccessCode header (invalid)
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtInsurant2, taskNotInProgressId,
        { {"X-AccessCode", "InvalidAccessCode"} }, { }, HttpStatus::Forbidden));

    // Doctor without AccessCode header:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtDoctor, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));
    // Doctor with AccessCode header (valid)
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtDoctor, taskNotInProgressId,
        { {"X-AccessCode", accessCode} }, { }, HttpStatus::NoContent));
    // Doctor with AccessCode header (invalid)
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtDoctor, taskNotInProgressId,
        { {"X-AccessCode", "Invalid"} }, { }, HttpStatus::Forbidden));
    // Doctor without AccessCode header but with "fake" JWT with id equal to KVNr of insurant (see https://dth01.ibmgcloud.net/jira/browse/ERP-5611):
    const auto jwtDoctorFake = JwtBuilder::testBuilder().makeJwtArzt(kvnr);
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtDoctorFake, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));

    // Pharmacy -> invalid status:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(operation, mServiceContext, jwtPharmacy, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));
}

TEST_F(EndpointHandlerTest, AbortTask169NotAllowed)
{
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$abort";

    const auto task= 4711;

    const std::string kvnr = "X234567890";
    // Insurant -> invalid status:
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(
        operation, mServiceContext, jwtInsurant1, task, {}, {}, HttpStatus::Forbidden, model::PrescriptionType::direkteZuweisung));
}

TEST_F(EndpointHandlerTest, RejectTask)//NOLINT(readability-function-cognitive-complexity)
{
    const auto jwt = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$reject";
    // not found
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, 999999999, { }, { }, HttpStatus::NotFound));
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0",
        { }, { }, HttpStatus::NotFound));

    // Task "in-progress":
    const auto taskInProgressId = 4716;

    // no secret:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, taskInProgressId, { }, { }, HttpStatus::Forbidden));
    // valid secret:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, taskInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::NoContent));
    // invalid secret:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, taskInProgressId, { },
        { {"secret", "invalid" } }, HttpStatus::Forbidden));

    // Task not "in-progress":
    const auto taskNotInProgressId = 4717;
    // valid secret, but invalid state:
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<RejectTaskHandler>(operation, mServiceContext, jwt, taskNotInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::Forbidden));
}

TEST_F(EndpointHandlerTest, MetaDataXml)//NOLINT(readability-function-cognitive-complexity)
{
    MetaDataHandler handler({});

    Header requestHeader{ HttpMethod::GET, "/metadata", 0, {{Header::Accept, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::optional<model::MetaData> metaData;
    ASSERT_NO_THROW(metaData = model::MetaData::fromXml(serverResponse.getBody(),
                                                        *StaticData::getXmlValidator(),
                                                        *StaticData::getInCodeValidator(),
                                                        SchemaType::fhir));
    EXPECT_EQ(metaData->version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(metaData->date(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));
    EXPECT_EQ(metaData->releaseDate(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData->setVersion(version);
    metaData->setDate(now);
    metaData->setReleaseDate(now);

    auto expectedMetaData =
            model::MetaData::fromXmlNoValidation(FileHelper::readFileAsString(dataPath + "/metadata.xml"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData->serializeToXmlString(), expectedMetaData.serializeToXmlString());
}

TEST_F(EndpointHandlerTest, MetaDataJson)//NOLINT(readability-function-cognitive-complexity)
{
    MetaDataHandler handler({});

    Header requestHeader{ HttpMethod::GET, "/metadata", 0, {{Header::Accept, ContentMimeType::fhirJsonUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    auto metaData = model::MetaData::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(metaData.jsonDocument()), SchemaType::fhir));

    EXPECT_EQ(metaData.version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(metaData.date(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));
    EXPECT_EQ(metaData.releaseDate(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData.setVersion(version);
    metaData.setDate(now);
    metaData.setReleaseDate(now);

    auto expectedMetaData =
        model::MetaData::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/metadata_1.1.1.json"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData.serializeToJsonString(), expectedMetaData.serializeToJsonString());
}

TEST_F(EndpointHandlerTest, DeviceXml)//NOLINT(readability-function-cognitive-complexity)
{
    DeviceHandler handler({});

    Header requestHeader{
        HttpMethod::GET, "/Device", 0,
        {{Header::Accept, ContentMimeType::fhirXmlUtf8}},
        HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::optional<model::Device> device;
    ASSERT_NO_THROW(device = model::Device::fromXml(serverResponse.getBody(),
                                                    *StaticData::getXmlValidator(),
                                                    *StaticData::getInCodeValidator(),
                                                    SchemaType::fhir));

    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->name(), model::Device::Name);
    EXPECT_TRUE(device->contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device->contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);
}

TEST_F(EndpointHandlerTest, DeviceJson)//NOLINT(readability-function-cognitive-complexity)
{
    DeviceHandler handler({});

    Header requestHeader{
        HttpMethod::GET, "/Device", 0,
        {{Header::Accept, ContentMimeType::fhirJsonUtf8}},
        HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    auto device = model::Device::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(device.jsonDocument()), SchemaType::fhir));

    EXPECT_EQ(device.status(), model::Device::Status::active);
    EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device.name(), model::Device::Name);
    EXPECT_TRUE(device.contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device.contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);
}

namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPostConsentHandler(
    std::optional<model::Consent>& resultConsent,
    PcServiceContext& serviceContext,
    JWT jwtInsurant,
    std::string consentJson,
    const HttpStatus expectedStatus)
{
    ConsentPostHandler handler({});
    Header requestHeader{
        HttpMethod::POST, "/Consent/", 0, {{Header::ContentType, ContentMimeType::fhirJsonUtf8}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwtInsurant));
    serverRequest.setBody(std::move(consentJson));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));

    if(expectedStatus == HttpStatus::Created)
    {
        ASSERT_NO_THROW(resultConsent = model::Consent::fromJson(
            serverResponse.getBody(), *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
            *StaticData::getInCodeValidator(), SchemaType::Gem_erxConsent));
        ASSERT_TRUE(resultConsent);
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, PostConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const auto& consentTemplateJson = ResourceManager::instance().getStringResource(dataPath + "/consent_template.json");
    const char* const origKvnr1 = "X500000000";
    auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(origKvnr1));
    const char* const origDateTimeStr = "2021-06-01T07:13:00+05:00";
    auto consentJson = String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr1), "##DATETIME##", origDateTimeStr);

    // Consent with same Kvnr already exists in mock database -> conflict
    A_22162.test("Only single consent for the same Kvnr");
    std::optional<model::Consent> resultConsent;
    ASSERT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant, consentJson, HttpStatus::Conflict));
    EXPECT_FALSE(resultConsent);

    const char* const origKvnr2 = "Y123456789";
    consentJson = String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr2), "##DATETIME##", origDateTimeStr);
    jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(origKvnr2));
    ASSERT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant, consentJson, HttpStatus::Created));

    EXPECT_EQ(resultConsent->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, origKvnr2));
    EXPECT_EQ(resultConsent->patientKvnr(), origKvnr2);
    EXPECT_TRUE(resultConsent->isChargingConsent());
    EXPECT_EQ(model::Timestamp::fromXsDateTime(origDateTimeStr), resultConsent->dateTime());

    // Check kvnr mismatch
    A_22289.test("Kvnr of access token and Consent patient identifier must be identical");
    resultConsent = {};
    EXPECT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant,
                                String::replaceAll(replaceKvnr(consentTemplateJson, "X999999999"), "##DATETIME##", origDateTimeStr),
                                HttpStatus::Forbidden));
    EXPECT_FALSE(resultConsent);

    // Check invalid Consent type
    EXPECT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant,
                                String::replaceAll(consentJson, "CHARGCONS", "Unsupported"),
                                HttpStatus::BadRequest));
    EXPECT_FALSE(resultConsent);

    // Check invalid datetime field
    EXPECT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant,
                                String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr2), "##DATETIME##", "2021-13-30T12:00:11+00:00"),
                                HttpStatus::BadRequest));
    EXPECT_FALSE(resultConsent);
}

namespace {

void checkDeleteConsentHandler(
    PcServiceContext& serviceContext,
    JWT jwtInsurant,
    const std::string& id,
    const HttpStatus expectedStatus)
{
    ConsentDeleteHandler handler({});
    Header requestHeader{HttpMethod::DELETE, "/Consent/" + id, 0, {{}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwtInsurant));
    if(!id.empty())
    {
        serverRequest.setPathParameters({"id"}, {id});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, DeleteConsent)//NOLINT(readability-function-cognitive-complexity)
{
    // Kvnr from static Consent object in mock database
    const char* const kvnr = "X500000000";
    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr));

    // succcessful deletion:
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteConsentHandler(mServiceContext, jwtInsurant,
                                  model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr),
                                  HttpStatus::NoContent));

    // deletion with unknown id fails (not found):
    const char* const unknownKvnr = "X999999999";
    const auto unknownId = model::Consent::createIdString(model::Consent::Type::CHARGCONS, unknownKvnr);
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteConsentHandler(mServiceContext,
                                  JwtBuilder::testBuilder().makeJwtVersicherter(std::string(unknownKvnr)),
                                  unknownId,
                                  HttpStatus::NotFound));

    // deletion without id is forbidden:
    A_22154.test("Deletion of Consent only by specific id");
    EXPECT_NO_FATAL_FAILURE(checkDeleteConsentHandler(mServiceContext, jwtInsurant, {}/*id*/, HttpStatus::Forbidden));

    // Kvnr mismatch betweeen id and JWT:
    A_22156.test("Kvnr of access token and id must match");
    EXPECT_NO_FATAL_FAILURE(checkDeleteConsentHandler(mServiceContext, jwtInsurant, unknownId, HttpStatus::Forbidden));
}

namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkGetConsentHandler(
    std::optional<model::Consent>& resultConsent,
    PcServiceContext& serviceContext,
    JWT jwtInsurant,
    const HttpStatus expectedStatus)
{
    ConsentGetHandler handler({});
    Header requestHeader{HttpMethod::GET, "/Consent/", 0, {{}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwtInsurant));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));

    if(expectedStatus == HttpStatus::OK)
    {
        std::optional<model::Bundle> consentBundle;
        ASSERT_NO_THROW(consentBundle = model::Bundle::fromJson(
           serverResponse.getBody(), *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
           *StaticData::getInCodeValidator(), SchemaType::fhir));
        ASSERT_TRUE(consentBundle);
        ASSERT_LE(consentBundle->getResourceCount(), 1);
        if(consentBundle->getResourceCount() == 1)
        {
            auto consents = consentBundle->getResourcesByType<model::Consent>("Consent");
            ASSERT_EQ(consents.size(), 1);
            ASSERT_NO_THROW(resultConsent = model::Consent::fromJson(
                consents.front().serializeToJsonString(), *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                *StaticData::getInCodeValidator(), SchemaType::Gem_erxConsent));
        }
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, GetConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const auto& consentTemplateJson = ResourceManager::instance().getStringResource(dataPath + "/consent_template.json");
    const char* const origKvnr = "X500000000";
    const char* const origDateTimeStr = "2021-06-01T07:13:00+05:00";
    // Consent object contained by mock database:
    auto consentJson = String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr), "##DATETIME##", origDateTimeStr);

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(origKvnr));

    // succcessful retrieval:
    A_22160.test("Filter Consent according to kvnr of insurant from access token");
    std::optional<model::Consent> resultConsent;
    EXPECT_NO_FATAL_FAILURE(checkGetConsentHandler(resultConsent, mServiceContext, jwtInsurant, HttpStatus::OK));
    ASSERT_TRUE(resultConsent);
    EXPECT_EQ(resultConsent->patientKvnr(), origKvnr);
    EXPECT_EQ(resultConsent->dateTime(), model::Timestamp::fromXsDateTime(origDateTimeStr));

    // empty result:
    resultConsent = {};
    EXPECT_NO_FATAL_FAILURE(checkGetConsentHandler(
        resultConsent, mServiceContext, JwtBuilder::testBuilder().makeJwtVersicherter("X999999999"), HttpStatus::OK));
    EXPECT_FALSE(resultConsent);

}

namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkGetChargeItemByIdHandler(
    PcServiceContext& serviceContext,
    JWT jwt,
    const std::string& id,
    const HttpStatus expectedStatus,
    const std::optional<std::string_view> accessCode = std::nullopt)
{
    ChargeItemGetByIdHandler handler({});
    Header requestHeader{HttpMethod::GET, "/ChargeItem/" + id, 0, {{}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    if(!id.empty())
    {
        serverRequest.setPathParameters({"id"}, {id});
    }

    if (accessCode.has_value())
    {
        serverRequest.setQueryParameters(
            std::vector<std::pair<std::string, std::string>>{std::make_pair("ac", accessCode->data())});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));

    if(expectedStatus == HttpStatus::OK)
    {
        const auto professionOIDClaim = serverRequest.getAccessToken().stringForClaim(JWT::professionOIDClaim);
        if(professionOIDClaim == profession_oid::oid_versicherter)
        {
            A_22128.test("Check ChargeItem.supportingInformation for insurant");

            const model::Bundle chargeItemBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
            const auto chargeItems = chargeItemBundle.getResourcesByType<model::ChargeItem>("ChargeItem");
            ASSERT_EQ(chargeItems.size(), 1);
            const auto& chargeItem = chargeItems[0];

            std::optional<model::ChargeItem> checkedChargeItem;
            ASSERT_NO_THROW(checkedChargeItem = model::ChargeItem::fromXml(
                chargeItem.serializeToXmlString(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                SchemaType::Gem_erxChargeItem));

            const auto bundleItems = chargeItemBundle.getResourcesByType<model::Bundle>("Bundle");
            ASSERT_EQ(bundleItems.size(), 3);

            const model::Bundle& dispenseItemBundle = bundleItems[0];
            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
            EXPECT_EQ(
                "Bundle/" + dispenseItemBundle.getId().toString(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).value());

            const model::Bundle& kbvBundle = bundleItems[1];
            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem).has_value());
            EXPECT_EQ(
                "Bundle/" + kbvBundle.getId().toString(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem).value());

            const model::Bundle& receipt = bundleItems[2];
            std::optional<model::ErxReceipt> checkedReceipt;
            ASSERT_NO_THROW(checkedReceipt = model::ErxReceipt::fromXml(
                receipt.serializeToXmlString(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                SchemaType::Gem_erxReceiptBundle));

            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt).has_value());
            EXPECT_EQ(
                "Bundle/" + receipt.getId().toString(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt).value());

            const auto& cadesBesTrustedCertDir = TestConfiguration::instance().getOptionalStringValue(
                    TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
            const auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);

            {
                // check KBV bundle signature
                const auto signature = kbvBundle.getSignature();
                ASSERT_TRUE(signature.has_value());
                std::string signatureData;
                ASSERT_NO_THROW(signatureData = signature->data().value().data());
                CadesBesSignature cms(certs, signatureData);
                std::optional<model::KbvBundle> kbvBundleFromSignature;
                ASSERT_NO_THROW(kbvBundleFromSignature = model::KbvBundle::fromXml(
                    cms.payload(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                    SchemaType::KBV_PR_ERP_Bundle));
                EXPECT_FALSE(kbvBundleFromSignature->getSignature().has_value());
            }
            {
                // check receipt signature
                const auto signature = receipt.getSignature();
                ASSERT_TRUE(signature.has_value());
                std::string signatureData;
                ASSERT_NO_THROW(signatureData = signature->data().value().data());
                CadesBesSignature cms(certs, signatureData);
                std::optional<model::ErxReceipt> receiptFromSignature;
                ASSERT_NO_THROW(receiptFromSignature = model::ErxReceipt::fromXml(
                    cms.payload(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                    SchemaType::Gem_erxReceiptBundle));
                EXPECT_FALSE(receiptFromSignature->getSignature().has_value());
            }
        }
        else
        {
            A_22128.test("Check ChargeItem.supportingInformation for pharmacy");
            const model::Bundle chargeItemBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
            const auto chargeItems = chargeItemBundle.getResourcesByType<model::ChargeItem>("ChargeItem");
            ASSERT_EQ(chargeItems.size(), 1);
            const auto& chargeItem = chargeItems[0];
            const auto bundleItems = chargeItemBundle.getResourcesByType<model::Bundle>("Bundle");

            ASSERT_EQ(bundleItems.size(), 1);

            EXPECT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
            ASSERT_EQ(
                "Bundle/" + bundleItems[0].getId().toString(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).value());
        }
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, GetChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const  kvnr = "X500000000";
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);

    A_22125.test("kvnr check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string("X999999999")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));

    A_22126.test("Telematik-ID check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtApotheke(std::string("606060606")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));

    A_22127.test("Check response for insurant");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                     pkvTaskId.toString(), HttpStatus::OK));

    A_22611.test("Access code check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                      pkvTaskId.toString(),
                                      HttpStatus::Forbidden));

    A_22128.test("Check response for pharmacy");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                      pkvTaskId.toString(),
                                      HttpStatus::OK,
                                      MockDatabase::mockAccessCode));
}

namespace {

void checkDeleteChargeItemHandler(
    PcServiceContext& serviceContext,
    JWT jwt,
    const std::string& id,
    const HttpStatus expectedStatus)
{
    ChargeItemDeleteHandler handler({});
    Header requestHeader{HttpMethod::DELETE, "/ChargeItem/" + id, 0, {{}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    if(!id.empty())
    {
        serverRequest.setPathParameters({"id"}, {id});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, DeleteChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const kvnr = "X500000000";
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);

    A_22112.test("Forbid all deletion");
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                     "", HttpStatus::Forbidden));

    A_22114.test("kvnr check");
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string("X999999999")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));

    A_22115.test("Telematik-ID check");
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtApotheke(std::string("606060606")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));

    A_22115.test("Delete chargeItem");
    // Insured
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                     pkvTaskId.toString(), HttpStatus::NoContent));

    // Pharmacy
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                     pkvTaskId.toString(), HttpStatus::NoContent));
}


namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPostChargeItemHandler(
    std::optional<model::ChargeItem>& resultChargeItem,
    PcServiceContext& serviceContext,
    JWT jwt,
    std::string body,
    const std::optional<model::PrescriptionId> prescriptionId,
    const std::optional<std::string_view> secret,
    const HttpStatus expectedStatus)
{
    ChargeItemPostHandler handler({});
    Header requestHeader{
        HttpMethod::POST, "/ChargeItem/", 0, {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setBody(std::move(body));

    std::vector<std::pair<std::string,std::string>> parameters;
    if(prescriptionId.has_value())
    {
        parameters.emplace_back("task", prescriptionId->toString());
    }
    if(secret.has_value())
    {
        parameters.emplace_back("secret", std::string(secret.value()));
    }
    serverRequest.setQueryParameters(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
    if(expectedStatus == HttpStatus::Created)
    {
        ASSERT_NO_THROW(resultChargeItem = model::ChargeItem::fromXml(
            serverResponse.getBody(), *StaticData::getXmlValidator(),
            *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));
        ASSERT_TRUE(resultChargeItem);
    }
}

} // anonymous namespace


TEST_F(EndpointHandlerTest, PostChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId= model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000000";

    auto& resourceManager = ResourceManager::instance();
    const auto dispenseBundleXml = resourceManager.getStringResource(dataPath + "/dispense_item.xml");
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(),
                                        CryptoHelper::cHpQesPrv(),
                                        dispenseBundleXml,
                                        std::nullopt};
    const auto chargeItemTemplateXml = resourceManager.getStringResource(dataPath + "/charge_item_POST_template.xml");
    const auto chargeItemXml =
        String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                           "##DISPENSE_BUNDLE##", Base64::encode(cadesBesSignature.get()));
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto taskTemplateJson = resourceManager.getStringResource(dataPath + "/task_pkv_closed_template.json");
    const auto taskJson = replaceKvnr(replacePrescriptionId(taskTemplateJson, pkvTaskId.toString()), pkvKvnr);
    const auto referencedTask = model::Task::fromJsonNoValidation(taskJson);

    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId()));

    // succcessful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), referencedTask.secret(),
        HttpStatus::Created));

    const auto inputDispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleXml);
    EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
    EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);

    A_22137.test("Contained binary removed and PKV dispense item reference set");
    EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
              "Bundle/"  + inputDispenseBundle.getId().toString());

    A_22134.test("KBV prescription bundle reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem),
              "Bundle/8938aff5-720a-414a-b574-114bd8d1e11c");

    A_22135.test("Receipt reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt),
              "Bundle/a0010000-0000-0000-0003-000000000000");

    // Error cases:

    A_22130.test("Check existence of 'task' parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, std::nullopt/*task id*/, referencedTask.secret(),
        HttpStatus::BadRequest));

    {
        A_22131.test("Check existance of referenced task");
        // Id of task which does not exist:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 999999999);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::Conflict));
    }
    {
        A_22131.test("Check task in status 'completed'");
        // Id of task which is not completed:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::Conflict));
    }

    A_22132.start("Check that secret is provided as URL parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), std::nullopt/*secret()*/,
        HttpStatus::Forbidden));
    A_22132.start("Check that secret from URL is equal to secret from referenced task");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), "invalidsecret",
        HttpStatus::Forbidden));

    {
        A_22136.test("Validate input ChargeItem resource: Kvnr");
        const char* const pkvKvnr = "X500000001";
        const auto chargeItemXml =
            String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                               "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22136.test("Validate input ChargeItem resource: TelematikId");
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("Invalid-TelematikId");
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22136.test("Validate input ChargeItem resource: PrescriptionId");
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22133.test("Check consent");
        const char* const pkvKvnr = "X500000001";
        const auto chargeItemXml =
            String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                               "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
}

TEST_F(EndpointHandlerTest, PostChargeItemNonQes)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkiPath = MockConfiguration::instance().getPathValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH);

    const auto nonQesSmcbCert = Certificate::fromPem(FileHelper::readFileAsString(
        pkiPath / "../tsl/X509Certificate/nonQesSmcb.pem"));
    const auto nonQesSmcbPrivateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(
        SafeString{FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/nonQesSmcbPrivateKey.pem")});

    auto nonQesSmcbCertX509 = X509Certificate::createFromX509Pointer(nonQesSmcbCert.toX509().removeConst().get());

    const auto ocspResponseData = mServiceContext.getTslManager().getCertificateOcspResponse(
        TslMode::TSL,
        nonQesSmcbCertX509,
        { CertificateType::C_HCI_OSIG },
        false);

    const auto pkvTaskId= model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000000";

    auto& resourceManager = ResourceManager::instance();
    const auto dispenseBundleXml = resourceManager.getStringResource(dataPath + "/dispense_item.xml");

    CadesBesSignature cadesBesSignature{nonQesSmcbCert,
                                        nonQesSmcbPrivateKey,
                                        dispenseBundleXml,
                                        std::nullopt,
                                        OcspHelper::stringToOcspResponse(ocspResponseData.response)};

    const auto chargeItemTemplateXml = resourceManager.getStringResource(dataPath + "/charge_item_POST_template.xml");
    const auto chargeItemXml =
        String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                           "##DISPENSE_BUNDLE##", Base64::encode(cadesBesSignature.get()));
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto taskTemplateJson = resourceManager.getStringResource(dataPath + "/task_pkv_closed_template.json");
    const auto taskJson = replaceKvnr(replacePrescriptionId(taskTemplateJson, pkvTaskId.toString()), pkvKvnr);
    const auto referencedTask = model::Task::fromJsonNoValidation(taskJson);

    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId()));

    // succcessful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), referencedTask.secret(),
        HttpStatus::Created));

    const auto inputDispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleXml);
    EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
    EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);

    A_22137.test("Contained binary removed and PKV dispense item reference set");
    EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
              "Bundle/"  + inputDispenseBundle.getId().toString());

    A_22134.test("KBV prescription bundle reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem),
              "Bundle/8938aff5-720a-414a-b574-114bd8d1e11c");

    A_22135.test("Receipt reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt),
              "Bundle/a0010000-0000-0000-0003-000000000000");

    // Error cases:

    A_22130.test("Check existence of 'task' parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, std::nullopt/*task id*/, referencedTask.secret(),
        HttpStatus::BadRequest));

    {
        A_22131.test("Check existance of referenced task");
        // Id of task which does not exist:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 999999999);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::Conflict));
    }
    {
        A_22131.test("Check task in status 'completed'");
        // Id of task which is not completed:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::Conflict));
    }

    A_22132.start("Check that secret is provided as URL parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), std::nullopt,
        HttpStatus::Forbidden));
    A_22132.start("Check that secret from URL is equal to secret from referenced task");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), "invalidsecret",
        HttpStatus::Forbidden));

    {
        A_22136.test("Validate input ChargeItem resource: Kvnr");
        const char* const pkvKvnr = "X500000001";
        const auto chargeItemXml =
            String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                               "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22136.test("Validate input ChargeItem resource: TelematikId");
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("Invalid-TelematikId");
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, inputChargeItem.prescriptionId(), referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22136.test("Validate input ChargeItem resource: PrescriptionId");
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
    {
        A_22133.test("Check consent");
        const char* const pkvKvnr = "X500000001";
        const auto chargeItemXml =
            String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                               "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml, pkvTaskId, referencedTask.secret(),
            HttpStatus::BadRequest));
    }
}


namespace {

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPutChargeItemHandler(
    std::optional<model::ChargeItem>& resultChargeItem,
    PcServiceContext& serviceContext,
    JWT jwt,
    const ContentMimeType& contentType,
    const model::ChargeItem& chargeItem,
    const std::optional<model::PrescriptionId>& id,
    const HttpStatus expectedStatus)
{
    const auto idStr = id.has_value() ? id->toString() : "";
    ChargeItemPutHandler handler({});
    Header requestHeader{HttpMethod::PUT, "/ChargeItem/" + idStr, 0, {{Header::ContentType, contentType}}, HttpStatus::Unknown};

    const auto accessCode = chargeItem.accessCode();
    if (accessCode.has_value())
    {
        requestHeader.addHeaderField(Header::XAccessCode, accessCode->data());
    }

    std::string body = (contentType.getMimeType() == MimeType::fhirJson) ?
        chargeItem.serializeToJsonString() : chargeItem.serializeToXmlString();

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setBody(std::move(body));

    if(id.has_value())
    {
        serverRequest.setPathParameters({"id"}, {idStr});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
    if(expectedStatus == HttpStatus::OK)
    {
        if(contentType.getMimeType() ==  MimeType::fhirJson)
        {
            ASSERT_NO_THROW(resultChargeItem = model::ChargeItem::fromJson(
                serverResponse.getBody(), *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));
        }
        else
        {
            ASSERT_NO_THROW(resultChargeItem = model::ChargeItem::fromXml(
                serverResponse.getBody(), *StaticData::getXmlValidator(),
                *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));
        }
        ASSERT_TRUE(resultChargeItem);
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkUnchangedChargeItemFieldsCommon(
    PcServiceContext& serviceContext,
    const JWT& jwt,
    const ContentMimeType& contentType,
    const std::string_view& chargeItemXml,
    const model::PrescriptionId& pkvTaskId)
{
    std::optional<model::ChargeItem> resultChargeItem;
    const auto errTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 99999);
    {
        // id
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setId(errTaskId);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // identifier
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setPrescriptionId(errTaskId);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Telematik id
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setEntererTelematikId("some-other-telematik-id");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Kvnr
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setSubjectKvnr("some-other-kvnr");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // entered date
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setEnteredDate(model::Timestamp::now());
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Receipt reference
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::receipt, "error-ref");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Prescription reference
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem, "error-ref");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Dispense reference
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.deleteSupportingInfoElement(model::ChargeItem::SupportingInfoType::dispenseItem);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, serviceContext, jwt, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, PutChargeItemPharmacy)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000000";

    auto &resourceManager = ResourceManager::instance();

    const auto newDispenseBundleXml = resourceManager.getStringResource(dataPath + "/dispense_item.xml");
    auto newDispenseBundle = model::Bundle::fromXmlNoValidation(newDispenseBundleXml);
    // set new ID to check update
    newDispenseBundle.setId(Uuid());
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(),
                                        CryptoHelper::cHpQesPrv(),
                                        newDispenseBundle.serializeToXmlString(),
                                        std::nullopt};
    const auto chargeItemTemplateXml = resourceManager.getStringResource(dataPath + "/charge_item_PUT_template.xml");
    const auto chargeItemXml =
         String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                            "##DISPENSE_BUNDLE##", Base64::encode(cadesBesSignature.get()));
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    {
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, contentType, inputChargeItem, pkvTaskId, HttpStatus::Forbidden));

        // succcessful retrieval:
        inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, contentType, inputChargeItem, pkvTaskId, HttpStatus::OK));
        EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
        EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);
        A_22148.test("Pharmacy: extract new PKV dispense item");
        EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
        EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
                  "Bundle/" + newDispenseBundle.getId().toString()); // updated to new id
    }
    {
        // Error: check match of telematik id from access token and ChargeItem
        A_22146.test("Pharmacy: validation of TelematikId");
        const auto errJwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("wrong-telematik-id");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, errJwtPharmacy, contentType, inputChargeItem, pkvTaskId, HttpStatus::Forbidden));
    }
    {
        // Error cases: Values other than dispense item changed in new ChargeItem
        A_22152.test("Check that ChargeItem fields other than dispense item are unchanged");
        ASSERT_NO_FATAL_FAILURE(checkUnchangedChargeItemFieldsCommon(
            mServiceContext, jwtPharmacy, contentType, chargeItemXml, pkvTaskId));
        {
            // Pharmacy: Marking
            const auto errChargeItemXml = String::replaceAll(chargeItemXml, "false", "true");
            auto errChargeItem = model::ChargeItem::fromXmlNoValidation(errChargeItemXml);
            errChargeItem.setAccessCode(MockDatabase::mockAccessCode);
            ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
                resultChargeItem, mServiceContext, jwtPharmacy, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));

            errChargeItem.deleteMarkingFlag();
            ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
                resultChargeItem, mServiceContext, jwtPharmacy, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
        }
    }
    {
        A_22215.test("Check consent");
        const char* const pkvKvnr = "X500000001";  // no consent exists for this kvnr
        const auto errChargeItemXml =
            String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                               "##DISPENSE_BUNDLE##", Base64::encode(newDispenseBundleXml));

        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(errChargeItemXml);
        errChargeItem.setAccessCode(MockDatabase::mockAccessCode);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, jwtPharmacy, contentType, errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
}

TEST_F(EndpointHandlerTest, PutChargeItemInsurant)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000000";

    auto &resourceManager = ResourceManager::instance();

    const auto dispenseBundleXml = resourceManager.getStringResource(dataPath + "/dispense_item.xml");
    const auto chargeItemTemplateXml = resourceManager.getStringResource(dataPath + "/charge_item_PUT_template.xml");
    const auto chargeItemXml =
        String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                           "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(pkvKvnr);

    const ContentMimeType contentType = ContentMimeType::fhirJsonUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    {
        // succcessful retrieval, change marking:
        EXPECT_FALSE(inputChargeItem.isMarked());
        const auto markedChargeItemXml = String::replaceAll(chargeItemXml, "false", "true");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, jwtInsurant, contentType,
            model::ChargeItem::fromXmlNoValidation(markedChargeItemXml), pkvTaskId, HttpStatus::OK));
        EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
        EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);
        EXPECT_TRUE(resultChargeItem->isMarked());
    }
    {
        // Error: check match of kvnr from access token and ChargeItem
        A_22145.test("Pharmacy: validation of Kvnr");
        const auto errJwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter("Z999999999");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, errJwtInsurant, contentType, inputChargeItem, pkvTaskId, HttpStatus::Forbidden));
    }
    {
        // Error cases: Values other than marking changed in new ChargeItem
        A_22152.test("Check that ChargeItem fields other than dispense item are unchanged");
        ASSERT_NO_FATAL_FAILURE(
            checkUnchangedChargeItemFieldsCommon(mServiceContext, jwtInsurant, contentType, chargeItemXml, pkvTaskId));
    }
    {
        A_22215.test("Check consent");
        const char* const pkvKvnr = "X500000001";  // no consent exists for this kvnr
        const auto errChargeItemXml =
                String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId.toString()), pkvKvnr),
                                   "##DISPENSE_BUNDLE##", Base64::encode(dispenseBundleXml));
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(
            resultChargeItem, mServiceContext, jwtInsurant, contentType,
            model::ChargeItem::fromXmlNoValidation(errChargeItemXml), pkvTaskId, HttpStatus::BadRequest));
    }
}
