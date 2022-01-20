/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpRequirements.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/server/context/ServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/DeviceHandler.hxx"
#include "erp/service/DosHandler.hxx"
#include "erp/service/MetaDataHandler.hxx"
#include "erp/service/task/TaskHandler.hxx"
#include "erp/service/task/AbortTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/StaticData.hxx"
#include "test_config.h"
#include "tools/jwt/JwtBuilder.hxx"
#include "tools/ResourceManager.hxx"
#include "test/mock/RegistrationMock.hxx"

#include <gtest/gtest.h>
#include <regex>
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

class EndpointHandlerTest : public testing::Test
{
public:
    EndpointHandlerTest()
        : dataPath(std::string{TEST_DATA_DIR} + "/EndpointHandlerTest"),
          mServiceContext(
              Configuration::instance(),
              [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
                  auto md = std::make_unique<MockDatabase>(hsmPool);
                  md->fillWithStaticData();
                  return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
              },
              std::make_unique<MockRedisStore>(),
              std::make_unique<HsmPool>(
                  std::make_unique<HsmMockFactory>(
                      std::make_unique<HsmMockClient>(),
                      MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
                  TeeTokenUpdater::createMockTeeTokenUpdaterFactory()),
              StaticData::getJsonValidator(),
              StaticData::getXmlValidator(),
              StaticData::getInCodeValidator(),
              std::make_unique<RegistrationMock>())
    {
        mJwtBuilder = std::make_unique<JwtBuilder>(MockCryptography::getIdpPrivateKey());
    }

    model::Task addTaskToDatabase(
        SessionContext<PcServiceContext>& sessionContext,
        model::Task::Status taskStatus,
        const std::string& accessCode,
        const std::string& insurant)
    {
        auto* database = sessionContext.database();
        model::Task task{ model::PrescriptionType::apothekenpflichigeArzneimittel, accessCode };
        task.setAcceptDate(model::Timestamp{ .0 });
        task.setExpiryDate(model::Timestamp{ .0 });
        task.setKvnr(insurant);
        task.updateLastUpdate(model::Timestamp{ .0 });
        task.setStatus(taskStatus);
        model::PrescriptionId prescriptionId = database->storeTask(task);
        task.setPrescriptionId(prescriptionId);
        if (taskStatus != model::Task::Status::draft)
        {
            task.setHealthCarePrescriptionUuid();
            const std::optional<std::string_view> healthCarePrescriptionUuid =
                task.healthCarePrescriptionUuid().value();
            const auto& kbvBundle = ResourceManager::instance().getStringResource(dataPath + "/kbv_bundle.xml");
            const model::Binary healthCarePrescriptionBundle{
                healthCarePrescriptionUuid.value(), CryptoHelper::toCadesBesSignature(kbvBundle)};
            database->activateTask(task, healthCarePrescriptionBundle);
        }
        database->commitTransaction();
        return task;
    }

    void checkGetAllAuditEvents(const std::string& kvnr, const std::string& expectedResultFilename)
    {
        GetAllAuditEventsHandler handler({});

        Header requestHeader{ HttpMethod::GET, "/AuditEvent/", 0, {}, HttpStatus::Unknown};

        auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        ServerRequest serverRequest{ std::move(requestHeader) };
        serverRequest.setAccessToken(std::move(jwt));

        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        const auto auditEventBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());

        ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(auditEventBundle.jsonDocument()),
            SchemaType::fhir));

        auto auditEvents = auditEventBundle.getResourcesByType<model::AuditEvent>("AuditEvent");
        ASSERT_EQ(auditEvents.size(), 1);

        auto& auditEvent = auditEvents.front();
        ASSERT_NO_THROW(model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator(),
                                                   *StaticData::getInCodeValidator(), SchemaType::Gem_erxAuditEvent));

        auto expectedAuditEvent = model::AuditEvent::fromJsonNoValidation(
            FileHelper::readFileAsString(dataPath + "/" + expectedResultFilename));

        ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
                  canonicalJson(expectedAuditEvent.serializeToJsonString()));
    }

protected:
    std::string dataPath;
    PcServiceContext mServiceContext;
    std::unique_ptr<JwtBuilder> mJwtBuilder;
};


TEST_F(EndpointHandlerTest, GetTaskById)
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

TEST_F(EndpointHandlerTest, GetTaskById169NoAccessCode)
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

TEST_F(EndpointHandlerTest, GetTaskByIdPatientConfirmation)
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

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingKVNR)
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

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeUrl)
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

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeHeader)
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

TEST_F(EndpointHandlerTest, GetTaskById_A20753_ExclusionOfVerificationIdentity)
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

TEST_F(EndpointHandlerTest, GetAllTasks)
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
TEST_F(EndpointHandlerTest, GetAllTasksErp6560)
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
        ASSERT_EQ(std::string(bundle.getLink(model::Link::Next).value()), "https://gematik.erppre.de:443/Task?_count=50&__offset=50");
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

TEST_F(EndpointHandlerTest, CreateTask)
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

    std::string parameters = R"--()--";

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

    std::string parameters = R"--()--";

    serverRequest.setBody(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
}

TEST_F(EndpointHandlerTest, ActivateTask)
{
    auto& resourceManager = ResourceManager::instance();
    const auto kbvBundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle.xml");
    auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(kbvBundle);

    const auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713).toString();

    ActivateTaskHandler handler({});
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$activate", 0, {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    requestHeader.addHeaderField("X-AccessCode", "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    ServerRequest serverRequest{ std::move(requestHeader) };

    std::string parameters = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +  cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    serverRequest.setBody(std::move(parameters));
    serverRequest.setPathParameters({"id"}, {id});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    std::optional<model::Task> task;
    ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(),
                                                *StaticData::getXmlValidator(),
                                                *StaticData::getInCodeValidator(),
                                                SchemaType::Gem_erxTask));
    ASSERT_TRUE(task.has_value());
    EXPECT_EQ(task->prescriptionId().toDatabaseId(), 4713);
    EXPECT_EQ(task->status(), model::Task::Status::ready);
    EXPECT_EQ(task->kvnr().value(), "X234567890");
    EXPECT_NO_THROW((void)task->expiryDate());
    EXPECT_FALSE(task->lastModifiedDate().toXsDateTime().empty());
    EXPECT_FALSE(task->authoredOn().toXsDateTime().empty());
    EXPECT_FALSE(task->accessCode().empty());
    EXPECT_FALSE(task->expiryDate().toXsDateTime().empty());
    EXPECT_FALSE(task->acceptDate().toXsDateTime().empty());
    EXPECT_TRUE(task->healthCarePrescriptionUuid().has_value());
    EXPECT_TRUE(task->patientConfirmationUuid().has_value());
    EXPECT_FALSE(task->receiptUuid().has_value());

    serverRequest.setPathParameters({ "id" }, { "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0" });
    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
}


TEST_F(EndpointHandlerTest, ActivateTaskBrokenSignature)
{
    const std::string cadesBesSignature; // empty signature

    const auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713).toString();

    ActivateTaskHandler handler({});
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$activate", 0, {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    requestHeader.addHeaderField("X-AccessCode", "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    ServerRequest serverRequest{ std::move(requestHeader) };

    std::string parameters = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +  cadesBesSignature + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    serverRequest.setBody(std::move(parameters));
    serverRequest.setPathParameters({"id"}, {id});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try
    {
        handler.handleRequest(sessionContext);
        FAIL();
    }
    catch(const ErpException& e)
    {
        ASSERT_EQ(HttpStatus::BadRequest, e.status());
    }
}


TEST_F(EndpointHandlerTest, CloseTask)
{
    const auto& testConfig = TestConfiguration::instance();
    auto medicationDispenseXml =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml");

    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();

    CloseTaskHandler handler({});
    const auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", id);
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$close/", 0,
                          {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({ { "secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } });
    serverRequest.setBody(std::move(medicationDispenseXml));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    ASSERT_NO_THROW((void) model::Bundle::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(), SchemaType::Gem_erxReceiptBundle));

    model::ErxReceipt receipt =
        model::ErxReceipt::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
                                   *StaticData::getInCodeValidator(), SchemaType::Gem_erxReceiptBundle);

    jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(); // need to be newly created as moved to access token

    const auto compositionResources = receipt.getResourcesByType<model::Composition>("Composition");
    const auto deviceReferencePath = "/Device/" + std::to_string(model::Device::Id);
    ASSERT_EQ(compositionResources.size(), 1);
    const auto& composition = compositionResources.front();
    EXPECT_NO_THROW(static_cast<void>(composition.id()));
    EXPECT_TRUE(composition.telematikId().has_value());
    EXPECT_EQ(composition.telematikId().value(), jwtPharmacy.stringForClaim(JWT::idNumberClaim).value());
    EXPECT_TRUE(composition.periodStart().has_value());
    EXPECT_EQ(composition.periodStart().value().toXsDateTime(), "2021-02-02T16:18:43.036+00:00");
    EXPECT_TRUE(composition.periodEnd().has_value());
    EXPECT_TRUE(composition.date().has_value());
    EXPECT_TRUE(composition.author().has_value());
    EXPECT_EQ(UrlHelper::parseUrl(std::string(composition.author().value())).mPath, deviceReferencePath);

    const auto deviceResources = receipt.getResourcesByType<model::Device>("Device");
    ASSERT_EQ(deviceResources.size(), 1);
    const auto& device = deviceResources.front();
    EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion);

    const auto prescriptionDigest = receipt.prescriptionDigest();
    EXPECT_EQ(prescriptionDigest.data(), "uqQu3nvsTNw7Gz97jkAuCzSebWyvZ4FZ5zE7TQTdxg0=");

    const auto signature = receipt.getSignature();
    ASSERT_TRUE(signature.has_value());
    EXPECT_TRUE(signature->when().has_value());
    EXPECT_TRUE(signature->data().has_value());
    EXPECT_TRUE(signature->who().has_value());
    EXPECT_EQ(UrlHelper::parseUrl(std::string(signature->who().value())).mPath, deviceReferencePath);

    std::string signatureData;
    ASSERT_NO_THROW(signatureData = signature->data().value().data());
    const auto& cadesBesTrustedCertDir =
        testConfig.getOptionalStringValue(TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
    auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
    CadesBesSignature cms(certs, signatureData);

    model::ErxReceipt receiptFromSignature = model::ErxReceipt::fromXmlNoValidation(cms.payload());
    EXPECT_FALSE(receiptFromSignature.getSignature().has_value());

    serverRequest.setPathParameters({ "id" }, { "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0" });
    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);


    std::string expectedFullUrl = "urn:uuid:" + std::string{composition.id()};
    rapidjson::Pointer fullUrlPtr("/entry/0/fullUrl");
    ASSERT_TRUE(
        fullUrlPtr.Get(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument())));
    ASSERT_EQ(
        std::string{
            fullUrlPtr.Get(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument()))
                ->GetString()},
        expectedFullUrl);
}

// Regression Test for Bugticket ERP-5656
TEST_F(EndpointHandlerTest, CloseTaskWrongMedicationDispenseErp5656)
{
    const auto correctId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    const auto wrongId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1111111).toString();
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    CloseTaskHandler handler({});
    Header requestHeader{ HttpMethod::POST, "/Task/" + correctId + "/$close/", 0,
                          {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { correctId });
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({ { "secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    { // wrong PrescriptionID
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", wrongId);
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    { // erroneous PrescriptionID
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", "falsch");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    { // wrong KVNR
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "X234567890", "X888888888");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    { // wrong Telematik-ID
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "3-SMC-B-Testkarte-883110000120312", "falsch");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
}

// Regression Test for Bugticket ERP-6513 (CloseTaskHandler does not accept MedicationDispense::whenPrepared and whenHandedOver with only date)
TEST_F(EndpointHandlerTest, CloseTaskPartialDateTimeErp6513)
{
    const auto correctId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    CloseTaskHandler handler({});
    Header requestHeader{ HttpMethod::POST, "/Task/" + correctId + "/$close/", 0,
                          {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { correctId });
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({ { "secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } });
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    // placeholder:
    //    <whenPrepared value="###WHENPREPARED###" />
    //    <whenHandedOver value="###WHENHANDEDOVER###" />
    {
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input_datetime_placeholder.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###WHENPREPARED###", "2020");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###WHENHANDEDOVER###", "2020-12");
        serverRequest.setBody(std::move(medicationDispenseXml));
        EXPECT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_NO_THROW(handler.handleRequest(sessionContext));
    }
}

TEST_F(EndpointHandlerTest, GetAllAuditEvents)
{
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446688", "audit_event.json"));
}

TEST_F(EndpointHandlerTest, GetAllAuditEvents_delete_task)
{
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446689", "audit_event_delete_task.json"));
}

TEST_F(EndpointHandlerTest, GetAuditEvent)
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

TEST_F(EndpointHandlerTest, AcceptTaskSuccess)
{
    auto& resourceManager = ResourceManager::instance();
    AcceptTaskHandler handler({});
    const auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setQueryParameters({ { "ac", "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea" } });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    std::optional<model::Bundle> bundle;
    ASSERT_NO_THROW(bundle = model::Bundle::fromXml(serverResponse.getBody(),
                                                    *StaticData::getXmlValidator(),
                                                    *StaticData::getInCodeValidator(),
                                                    SchemaType::fhir));
    ASSERT_EQ(bundle->getResourceCount(), 2);

    const auto tasks = bundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), id);
    ASSERT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    ASSERT_TRUE(tasks[0].secret().has_value());

    const auto binaryResources = bundle->getResourcesByType<model::Binary>("Binary");
    ASSERT_EQ(binaryResources.size(), 1);
    ASSERT_TRUE(binaryResources[0].data().has_value());
    std::optional<CadesBesSignature> signature;
    ASSERT_NO_THROW(signature.emplace(std::string{binaryResources[0].data().value()}, nullptr));
    const auto& prescription = resourceManager.getStringResource(dataPath + "/kbv_bundle.xml");
    ASSERT_EQ(signature->payload(), prescription);

    ASSERT_TRUE(tasks[0].healthCarePrescriptionUuid().has_value());
    ASSERT_TRUE(binaryResources[0].id().has_value());
    ASSERT_EQ(tasks[0].healthCarePrescriptionUuid().value(), binaryResources[0].id().value());
}

TEST_F(EndpointHandlerTest, AcceptTaskFail)
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
    AbortTaskHandler handler({});
    const auto id = getId(taskId, type);
    Header requestHeader{ HttpMethod::POST, "/Task/" + id + "/" + operationName + "/", 0, std::move(headers), HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setQueryParameters(std::move(queryParameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    HttpStatus status = HttpStatus::Unknown;
    try
    {
        handler.handleRequest(sessionContext);
        status = serverResponse.getHeader().status();
    }
    catch(const ErpException& exc)
    {
        status = exc.status();
    }
    ASSERT_EQ(status, expectedStatus);
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, AbortTask)
{
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$abort";
    // not found
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtPharmacy, 999999999, { }, { }, HttpStatus::NotFound));
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtPharmacy, "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0", { }, { }, HttpStatus::NotFound));

    // Task "in-progress":
    const auto taskInProgressId = 4716;

    // Pharmacy, no secret:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtPharmacy, taskInProgressId, { }, { }, HttpStatus::Forbidden));
    // Pharmacy, valid secret:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtPharmacy, taskInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::NoContent));

    const std::string kvnr = "X234567890";
    // Insurant -> invalid status:
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant1, taskInProgressId, { }, { }, HttpStatus::Forbidden));

    // Doctor -> invalid status:
    const auto jwtDoctor = JwtBuilder::testBuilder().makeJwtArzt();
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtDoctor, taskInProgressId, { }, { }, HttpStatus::Forbidden));

    // Task not "in-progress":
    const auto taskNotInProgressId = 4717;

    // Insurant without AccessCode header -> kvnr check (valid):
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant1, taskNotInProgressId, { }, { }, HttpStatus::NoContent));
    // Insurant without AccessCode header -> kvnr check (invalid):
    const auto jwtInsurant2 = JwtBuilder::testBuilder().makeJwtVersicherter("Unknown");
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant2, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));

    const std::string accessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    // Insurant with AccessCode header (valid)
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant2, taskNotInProgressId,
        { {"X-AccessCode", accessCode} }, { }, HttpStatus::NoContent));
    // Insurant with AccessCode header (invalid)
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant2, taskNotInProgressId,
        { {"X-AccessCode", "InvalidAccessCode"} }, { }, HttpStatus::Forbidden));

    // Doctor without AccessCode header:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtDoctor, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));
    // Doctor with AccessCode header (valid)
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtDoctor, taskNotInProgressId,
        { {"X-AccessCode", accessCode} }, { }, HttpStatus::NoContent));
    // Doctor with AccessCode header (invalid)
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtDoctor, taskNotInProgressId,
        { {"X-AccessCode", "Invalid"} }, { }, HttpStatus::Forbidden));
    // Doctor without AccessCode header but with "fake" JWT with id equal to KVNr of insurant (see https://dth01.ibmgcloud.net/jira/browse/ERP-5611):
    const auto jwtDoctorFake = JwtBuilder::testBuilder().makeJwtArzt(kvnr);
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtDoctorFake, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));

    // Pharmacy -> invalid status:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtPharmacy, taskNotInProgressId, { }, { }, HttpStatus::Forbidden));
}

TEST_F(EndpointHandlerTest, AbortTask169NotAllowed)
{
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$abort";

    const auto task= 4711;

    const std::string kvnr = "X234567890";
    // Insurant -> invalid status:
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwtInsurant1, task, {}, {}, HttpStatus::Forbidden,
                                       model::PrescriptionType::direkteZuweisung));
}

TEST_F(EndpointHandlerTest, RejectTask)
{
    const auto jwt = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$reject";
    // not found
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, 999999999, { }, { }, HttpStatus::NotFound));
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, "9a27d600-5a50-4e2b-98d3-5e05d2e85aa0", { }, { }, HttpStatus::NotFound));

    // Task "in-progress":
    const auto taskInProgressId = 4716;

    // no secret:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, taskInProgressId, { }, { }, HttpStatus::Forbidden));
    // valid secret:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, taskInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::NoContent));
    // invalid secret:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, taskInProgressId, { },
        { {"secret", "invalid" } }, HttpStatus::Forbidden));

    // Task not "in-progress":
    const auto taskNotInProgressId = 4717;
    // valid secret, but invalid state:
    EXPECT_NO_THROW(checkTaskOperation(operation, mServiceContext, jwt, taskNotInProgressId, { },
        { {"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" } }, HttpStatus::Forbidden));
}

TEST_F(EndpointHandlerTest, MetaDataXml)
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

TEST_F(EndpointHandlerTest, MetaDataJson)
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
        model::MetaData::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/metadata.json"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData.serializeToJsonString(), expectedMetaData.serializeToJsonString());
}

TEST_F(EndpointHandlerTest, DeviceXml)
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

TEST_F(EndpointHandlerTest, DeviceJson)
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
