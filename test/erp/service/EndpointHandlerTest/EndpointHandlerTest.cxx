/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/DeviceHandler.hxx"
#include "erp/service/MetaDataHandler.hxx"
#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"
#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/service/consent/ConsentPostHandler.hxx"
#include "erp/service/task/AbortTaskHandler.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/service/task/RejectTaskHandler.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/hsm/production/ProductionBlobDatabase.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/ErpMacros.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test_config.h"

#include <gtest/gtest.h>
#include <variant>

namespace fs = std::filesystem;

using namespace ::std::literals;


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
 * When the mock database is created it is also filled with static data.
 * See function MockDatabase::fillWithStaticData() for the exact data which is written into the database.
 *
*/


TEST_F(EndpointHandlerTest, GetTaskById)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    const auto idStr = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711).toString();

    Header requestHeader{ HttpMethod::GET, "/Task/" + idStr, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456788"));
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
    ASSERT_NO_THROW((void) model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator()));
    ASSERT_EQ(std::string(rapidjson::Pointer("/resourceType").Get(document)->GetString()), "Bundle");
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
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456788"));
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

    ASSERT_NO_THROW((void) model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator()));

    ASSERT_THROW((void)tasks[0].accessCode(), model::ModelException);

}

TEST_F(EndpointHandlerTest, GetTaskByIdPatientConfirmation)//NOLINT(readability-function-cognitive-complexity)
{
    GetTaskHandler handler({});

    const auto idStr = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();

    Header requestHeader{ HttpMethod::GET, "/Task/" + idStr, 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X234567891"));
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

    fhirtools::ValidatorOptions kbvValidatorOptions{.allowNonLiteralAuthorReference = true};
    std::optional<model::ResourceFactory<model::KbvBundle>> kbvBundleFactory;

    ASSERT_NO_THROW(kbvBundleFactory.emplace(model::ResourceFactory<model::KbvBundle>::fromXml(
        prescriptions[0].serializeToXmlString(), *StaticData::getXmlValidator(),
        {.validatorOptions = kbvValidatorOptions})));
    ASSERT_TRUE(prescriptions[0].getSignature().has_value());
    std::optional<model::KbvBundle> kbvBundle;
    ASSERT_NO_THROW(
        kbvBundle.emplace(std::move(*kbvBundleFactory).getValidated(model::ProfileType::KBV_PR_ERP_Bundle)));
    ASSERT_TRUE(prescriptions[0].getSignature().has_value());
    auto signature = prescriptions[0].getSignature().value();
    ASSERT_TRUE(signature.when().has_value());
    ASSERT_TRUE(signature.whoReference().has_value());
    EXPECT_EQ(UrlHelper::parseUrl(std::string(signature.whoReference().value())).mPath,
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

// GEMREQ-start A_19116-01
TEST_F(EndpointHandlerTest, GetTaskByIdMatchingKVNR)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116_01.test("Get Task using the correct KVNR");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456788
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{ HttpMethod::GET, "/Task/160.000.000.004.711.86", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456788"));
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
    ASSERT_NO_THROW((void) model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator()));
}

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeUrl)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116_01.test("Get Task wrong KVNR and using the correct AccessCode passed by URL");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456788
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{ HttpMethod::GET, "/Task/160.000.000.004.711.86", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654326"));
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
    ASSERT_NO_THROW((void) model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator()));
}

TEST_F(EndpointHandlerTest, GetTaskByIdMatchingAccessCodeHeader)//NOLINT(readability-function-cognitive-complexity)
{
    A_19116_01.test("Get Task wrong KVNR and using the correct AccessCode passed by Http-Header");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456788
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
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654326"));
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
    ASSERT_NO_THROW((void) model::Task::fromXml(tasks[0].serializeToXmlString(), *StaticData::getXmlValidator()));
}

TEST_F(EndpointHandlerTest, GetTaskByIdWrongAccessCodeHeader)
{
    A_19116_01.test("Get Task wrong KVNR and using the wrong AccessCode");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456788
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{HttpMethod::GET,
                         "/Task/160.000.000.004.711.86",
                         0,
                         {{Header::XAccessCode, "wrong_access_code"}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654326"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
    ASSERT_TRUE(serverResponse.getBody().empty());
}

TEST_F(EndpointHandlerTest, GetTaskByIdMissingAccessCode)
{
    A_19116_01.test("Get Task using wrong KVNR and without AccessCode");
    // The database contains a task with
    // id: 160.000.000.004.711.86
    // kvnr: X123456788
    // AccessCode: 777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea

    GetTaskHandler handler({});

    // Mock the necessary session information
    Header requestHeader{HttpMethod::GET,
                         "/Task/160.000.000.004.711.86",
                         0,
                         {},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X987654326"));
    serverRequest.setPathParameters({ "id" }, { "160.000.000.004.711.86" });

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
    ASSERT_TRUE(serverResponse.getBody().empty());
}
// GEMREQ-end A_19116-01

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
        std::string kvnrInsurant = "X123456788";
        std::string kvnrTestCard = "X000022222";
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
             "idNummer":      "X123456788"
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
        ASSERT_NO_THROW((void) model::Task::fromXml(item.serializeToXmlString(), *StaticData::getXmlValidator()));
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
             "idNummer":      "X123456788"
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
            addTaskToDatabase(sessionContext, model::Task::Status::ready, "access-code", "X123456788");
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
        ASSERT_EQ(val, "https://gematik.erppre.de:443/Task?_sort=authored-on&_count=50&__offset=50");
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
        ASSERT_EQ(std::string(bundle2.getLink(model::Link::Next).value()), "https://gematik.erppre.de:443/Task?_sort=authored-on&_count=50&__offset=100");
        ASSERT_TRUE(bundle2.getLink(model::Link::Prev).has_value());
        ASSERT_EQ(std::string(bundle2.getLink(model::Link::Prev).value()), "https://gematik.erppre.de:443/Task?_sort=authored-on&_count=50&__offset=0");


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
        ASSERT_EQ(std::string(bundle3.getLink(model::Link::Prev).value()), "https://gematik.erppre.de:443/Task?_sort=authored-on&_count=50&__offset=50");
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
      <system value=")--" + std::string(model::resource::code_system::flowType) +
                             R"--("/>
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
    ASSERT_NO_THROW(
        retrievedTask.emplace(model::Task::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator())));
    EXPECT_GT(retrievedTask->prescriptionId().toDatabaseId(), 0);
    EXPECT_EQ(retrievedTask->status(), model::Task::Status::draft);
    EXPECT_FALSE(retrievedTask->kvnr().has_value());
    EXPECT_EQ(retrievedTask->type(), model::PrescriptionType::apothekenpflichigeArzneimittel);
    const auto ac = retrievedTask->accessCode();
    EXPECT_EQ(ac.size(), 64);
}

class EndpointHandlerSampleTest : public EndpointHandlerTest, public ::testing::WithParamInterface<std::string>
{
public:
    using EndpointHandlerTest::EndpointHandlerTest;
};


TEST_P(EndpointHandlerSampleTest, CreateTaskJson)
{
    CreateTaskHandler handler({});

    Header requestHeader{ HttpMethod::POST, "/Task/$create", 0, {{Header::ContentType,ContentMimeType::fhirJsonUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };

    serverRequest.setBody(GetParam());

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
}

TEST_P(EndpointHandlerSampleTest, CreateTaskXml)
{
    CreateTaskHandler handler({});

    Header requestHeader{ HttpMethod::POST, "/Task/$create", 0, {{Header::ContentType,ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };

    serverRequest.setBody(GetParam());

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
}

INSTANTIATE_TEST_SUITE_P(invalid, EndpointHandlerSampleTest,
                         ::testing::Values( "", "-", "+", "{", "{}", "<?>", "\0{}", "\0djjdj", "\n", "\r\n"));

TEST_F(EndpointHandlerTest, GetAllAuditEvents)
{
    const std::string gematikVersionStr{to_string(ResourceTemplates::Versions::GEM_ERP_current())};
    const std::string auditEventFileName = "audit_event_" + gematikVersionStr + ".json";
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446685", auditEventFileName));
}

TEST_F(EndpointHandlerTest, GetAllAuditEvents_delete_task)
{
    const std::string gematikVersionStr{to_string(ResourceTemplates::Versions::GEM_ERP_current())};
    const std::string auditEventFileName = "audit_event_delete_task_" + gematikVersionStr + ".json";
    ASSERT_NO_FATAL_FAILURE(checkGetAllAuditEvents("X122446697", auditEventFileName));
}

TEST_F(EndpointHandlerTest, GetAuditEvent)//NOLINT(readability-function-cognitive-complexity)
{
    GetAuditEventHandler handler({});

    const std::string id = "01eb69a4-9029-d610-b1cf-ddb8c6c0bfbc";
    Header requestHeader{ HttpMethod::GET, "/AuditEvent/" + id, 0, { {Header::AcceptLanguage, "de"} }, HttpStatus::Unknown};

    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X122446685");
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

    ASSERT_NO_THROW(
        (void) model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator()));

    const std::string gematikVersionStr{to_string(ResourceTemplates::Versions::GEM_ERP_current())};
    const std::string auditEventFileName = dataPath + "/audit_event_" + gematikVersionStr + ".json";
    auto expectedAuditEvent =
        model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(auditEventFileName));

    ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
              canonicalJson(expectedAuditEvent.serializeToJsonString()));
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


}// anonymous namespace

class TaskOperationEndpointTest : public EndpointHandlerTest
{
protected:
    template<class HandlerType>
    void checkTaskOperation(const std::string& operationName, PcServiceContext& serviceContext, JWT jwt,
                            const std::variant<int64_t, std::string>& taskId, Header::keyValueMap_t&& headers,
                            QueryParameters&& queryParameters, const HttpStatus expectedStatus,
                            model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel)
    {
        mockDatabase.reset();
        HandlerType handler({});
        const auto id = getId(taskId, type);
        Header requestHeader{HttpMethod::POST, "/Task/" + id + "/" + operationName + "/", 0, std::move(headers),
                             HttpStatus::Unknown};

        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setAccessToken(std::move(jwt));
        serverRequest.setQueryParameters(std::move(queryParameters));

        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
    }
};

TEST_F(TaskOperationEndpointTest, AbortTask)//NOLINT(readability-function-cognitive-complexity)
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

    const std::string kvnr = "X234567891";
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

TEST_F(TaskOperationEndpointTest, AbortTask169NotAllowed)
{
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string operation = "$abort";

    const auto task= 4711;

    const std::string kvnr = "X234567891";
    // Insurant -> invalid status:
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    EXPECT_NO_FATAL_FAILURE(checkTaskOperation<AbortTaskHandler>(
        operation, mServiceContext, jwtInsurant1, task, {}, {}, HttpStatus::Forbidden, model::PrescriptionType::direkteZuweisung));
}

TEST_F(TaskOperationEndpointTest, RejectTask)//NOLINT(readability-function-cognitive-complexity)
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

    LOG(INFO) << serverResponse.getBody();
    std::optional<model::MetaData> metaData;
    ASSERT_NO_THROW(metaData = model::MetaData::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()));
    EXPECT_EQ(metaData->version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(metaData->date(), model::Timestamp::fromXsDateTime(std::string{ErpServerInfo::ReleaseDate()}));
    EXPECT_EQ(metaData->releaseDate(), model::Timestamp::fromXsDateTime(std::string{ErpServerInfo::ReleaseDate()}));

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData->setVersion(version);
    metaData->setDate(now);
    metaData->setReleaseDate(now);

    const std::string gematikVer{to_string(ResourceTemplates::Versions::GEM_ERP_current())};
    auto expectedMetaData =
            model::MetaData::fromXmlNoValidation(FileHelper::readFileAsString(dataPath + "/metadata_" + gematikVer + ".xml"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData->serializeToXmlString(), expectedMetaData.serializeToXmlString()) << metaData->serializeToXmlString();
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

    std::optional<model::MetaData> metaData;
    ASSERT_NO_THROW(metaData = model::MetaData::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));

    EXPECT_EQ(metaData->version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(metaData->date(), model::Timestamp::fromXsDateTime(std::string{ErpServerInfo::ReleaseDate()}));
    EXPECT_EQ(metaData->releaseDate(), model::Timestamp::fromXsDateTime(std::string{ErpServerInfo::ReleaseDate()}));

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData->setVersion(version);
    metaData->setDate(now);
    metaData->setReleaseDate(now);

    const std::string gematikVer{to_string(ResourceTemplates::Versions::GEM_ERP_current())};
    auto expectedMetaData =
        model::MetaData::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/metadata_" + gematikVer + ".json"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData->serializeToJsonString(), expectedMetaData.serializeToJsonString());
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
    ASSERT_NO_THROW(device = model::Device::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()));

    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion());
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
    EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion());
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

    ASSERT_NO_FATAL_FAILURE(
        EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));

    if(expectedStatus == HttpStatus::Created)
    {
        ASSERT_NO_THROW(resultConsent =
                            model::Consent::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));
        ASSERT_TRUE(resultConsent);
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, PostConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const auto& consentTemplateJson = ResourceManager::instance().getStringResource(dataPath + "/consent_template.json");
    const char* const origKvnr1 = "X500000056";
    auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(origKvnr1));
    const char* const origDateTimeStr = "2021-06-01T07:13:00+05:00";
    auto consentJson = String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr1), "##DATETIME##", origDateTimeStr);

    // Consent with same Kvnr already exists in mock database -> conflict
    A_22162.test("Only single consent for the same Kvnr");
    std::optional<model::Consent> resultConsent;
    ASSERT_NO_FATAL_FAILURE(
        checkPostConsentHandler(resultConsent, mServiceContext, jwtInsurant, consentJson, HttpStatus::Conflict));
    EXPECT_FALSE(resultConsent);

    const model::Kvnr origKvnr2{"Y123456781"};
    consentJson = String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr2.id()), "##DATETIME##", origDateTimeStr);
    jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(origKvnr2);
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
                                String::replaceAll(replaceKvnr(consentTemplateJson, "X999999992"), "##DATETIME##", origDateTimeStr),
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
                                String::replaceAll(replaceKvnr(consentTemplateJson, origKvnr2.id()), "##DATETIME##", "2021-13-30T12:00:11+00:00"),
                                HttpStatus::BadRequest));
    EXPECT_FALSE(resultConsent);
}

namespace {

void checkDeleteConsentHandler(
    PcServiceContext& serviceContext,
    JWT jwtInsurant,
    const HttpStatus expectedStatus,
    const std::optional<std::string_view> categoryValue = "CHARGCONS")
{
    ConsentDeleteHandler handler({});

    Header requestHeader{HttpMethod::DELETE, "/Consent", 0, {{}}, HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwtInsurant));
    if (categoryValue.has_value())
    {
        serverRequest.setQueryParameters({std::make_pair("category", categoryValue->data())});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(
        EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, DeleteConsent)//NOLINT(readability-function-cognitive-complexity)
{
    // Kvnr from static Consent object in mock database
    const char* const kvnr = "X500000056";
    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr));

    A_22154.test("Query parameter category must exist");
    EXPECT_NO_FATAL_FAILURE(checkDeleteConsentHandler(
        mServiceContext,
        jwtInsurant,
        HttpStatus::MethodNotAllowed,
        std::nullopt));

    A_22874_01.test("Query parameter category must have correct value");
    EXPECT_NO_FATAL_FAILURE(checkDeleteConsentHandler(
        mServiceContext,
        jwtInsurant,
        HttpStatus::BadRequest,
        "category=BAD"));

    A_22158.test("Deletion with unknown id fails (not found)");
    const char* const unknownKvnr = "X999999992";
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteConsentHandler(mServiceContext,
                                  JwtBuilder::testBuilder().makeJwtVersicherter(std::string(unknownKvnr)),
                                  HttpStatus::NotFound));

    A_22157.test("successful deletion");
    EXPECT_NO_FATAL_FAILURE(checkDeleteConsentHandler(mServiceContext, jwtInsurant, HttpStatus::NoContent));
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

    ASSERT_NO_FATAL_FAILURE(
        EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));

    if(expectedStatus == HttpStatus::OK)
    {
        std::optional<model::Bundle> consentBundle;
        const auto& fhirInstance = Fhir::instance();
        const auto& backend = fhirInstance.backend();
        auto viewList = fhirInstance.structureRepository(model::Timestamp::now());
        auto view = viewList.match(&backend, std::string{model::resource::structure_definition::consent},
                                   ResourceTemplates::Versions::GEM_ERPCHRG_current());
        ASSERT_NO_THROW(consentBundle.emplace(
            model::ResourceFactory<model::Bundle>::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator())
                .getValidated(model::ProfileType::fhir, view)));
        ASSERT_TRUE(consentBundle);
        ASSERT_LE(consentBundle->getResourceCount(), 1);
        if(consentBundle->getResourceCount() == 1)
        {
            auto consents = consentBundle->getResourcesByType<model::Consent>("Consent");
            ASSERT_EQ(consents.size(), 1);
            ASSERT_NO_THROW(resultConsent = model::Consent::fromJson(consents.front().serializeToJsonString(),
                                                                     *StaticData::getJsonValidator()));
        }
    }
}

} // anonymous namespace

TEST_F(EndpointHandlerTest, GetConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const origKvnr = "X500000056";
    const char* const origDateTimeStr = "2021-06-01T07:13:00+05:00";
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
        resultConsent, mServiceContext, JwtBuilder::testBuilder().makeJwtVersicherter("X999999992"), HttpStatus::OK));
    EXPECT_FALSE(resultConsent);
}

namespace {

// GEMREQ-start checkGetChargeItemByIdHandler
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

    ASSERT_NO_FATAL_FAILURE(
        EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
    // GEMREQ-end checkGetChargeItemByIdHandler

    if(expectedStatus == HttpStatus::OK)
    {
        // GEMREQ-start A_22127#validateBundle
        const auto professionOIDClaim = serverRequest.getAccessToken().stringForClaim(JWT::professionOIDClaim);
        if(professionOIDClaim == profession_oid::oid_versicherter)
        {
            const model::Bundle chargeItemBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
            const auto chargeItems = chargeItemBundle.getResourcesByType<model::ChargeItem>("ChargeItem");
            ASSERT_EQ(chargeItems.size(), 1);
            const auto& chargeItem = chargeItems[0];

            std::optional<model::ChargeItem> checkedChargeItem;
            ASSERT_NO_THROW(checkedChargeItem = model::ChargeItem::fromXml(chargeItem.serializeToXmlString(),
                                                                           *StaticData::getXmlValidator()));
            EXPECT_FALSE(checkedChargeItem->containedBinary());

            const auto bundleItems = chargeItemBundle.getResourcesByType<model::Bundle>("Bundle");
            ASSERT_EQ(bundleItems.size(), 3);

            const model::Bundle& dispenseItemBundle = bundleItems[0];
            EXPECT_FALSE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary).has_value());
            EXPECT_FALSE(chargeItem.containedBinary().has_value());
            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).has_value());
            EXPECT_EQ(
                Uuid{chargeItem.prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).value());

            const model::Bundle& kbvBundle = bundleItems[1];
            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).has_value());
            EXPECT_EQ(
                Uuid{chargeItem.prescriptionId()->deriveUuid(model::uuidFeaturePrescription)}.toUrn(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).value());

            const model::Bundle& receipt = bundleItems[2];
            std::optional<model::ErxReceipt> checkedReceipt;
            ASSERT_NO_THROW(checkedReceipt.emplace(
                model::ErxReceipt::fromXml(receipt.serializeToXmlString(), *StaticData::getXmlValidator())));

            ASSERT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle).has_value());
            EXPECT_EQ(
                Uuid{chargeItem.prescriptionId()->deriveUuid(model::uuidFeatureReceipt)}.toUrn(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle).value());

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
                constexpr fhirtools::ValidatorOptions kbvValidatorOptions{.allowNonLiteralAuthorReference = true};
                using KbvBundleFactory = model::ResourceFactory<model::KbvBundle>;
                std::optional<KbvBundleFactory> kbvBundleFactory;
                ASSERT_NO_THROW(kbvBundleFactory.emplace(KbvBundleFactory::fromXml(
                    cms.payload(), *StaticData::getXmlValidator(), {.validatorOptions = kbvValidatorOptions})));
                std::optional<model::KbvBundle> kbvBundleFromSignature;
                ASSERT_NO_THROW(kbvBundleFromSignature.emplace(
                    std::move(*kbvBundleFactory).getValidated(model::ProfileType::KBV_PR_ERP_Bundle)));
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
                ASSERT_NO_THROW(receiptFromSignature.emplace(
                    model::ErxReceipt::fromXml(cms.payload(), *StaticData::getXmlValidator())));
                EXPECT_FALSE(receiptFromSignature->getSignature().has_value());
            }

            {
                // counter signature for dispense item
                const auto signature = dispenseItemBundle.getSignature();
                ASSERT_TRUE(signature.has_value());
                std::string signatureDataBase64;
                ASSERT_NO_THROW(signatureDataBase64 = signature->data().value().data());
                const CadesBesSignature cms(signatureDataBase64);
                ASSERT_NO_THROW(cms.validateCounterSignature({serviceContext.getCFdSigErp()}));
                ASSERT_NO_THROW((void)model::AbgabedatenPkvBundle::fromXmlNoValidation(cms.payload()));
            }
        }
        // GEMREQ-end A_22127#validateBundle
        else
        {
            A_22128_01.test("Check ChargeItem.supportingInformation for pharmacy");
            const model::Bundle chargeItemBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
            const auto chargeItems = chargeItemBundle.getResourcesByType<model::ChargeItem>("ChargeItem");
            ASSERT_EQ(chargeItems.size(), 1);
            const auto& chargeItem = chargeItems[0];
            EXPECT_FALSE(chargeItem.containedBinary());
            EXPECT_FALSE(chargeItem.accessCode());

            const auto bundleItems = chargeItemBundle.getResourcesByType<model::Bundle>("Bundle");

            ASSERT_EQ(bundleItems.size(), 2); // prescription and dispense item

            EXPECT_TRUE(
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).has_value());
            ASSERT_EQ(
                Uuid{chargeItem.prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn(),
                chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).value());
        }
    }
}

} // anonymous namespace

// GEMREQ-start A_22126#start
// GEMREQ-start A_22125, A_22127
TEST_F(EndpointHandlerTest, GetChargeItemById)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const kvnr = "X500000056";
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    // GEMREQ-end A_22126#start

    A_22127.test("Check response for insurant");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                      pkvTaskId.toString(), HttpStatus::OK));
    // GEMREQ-end A_22127

    A_22125.test("kvnr check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string("X999999992")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));
// GEMREQ-end A_22125

// GEMREQ-start A_22126#check
    A_22128_01.test("Check response for pharmacy");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                      pkvTaskId.toString(),
                                      HttpStatus::OK,
                                      MockDatabase::mockAccessCode));

    A_22126.test("Telematik-ID check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtApotheke(std::string("606060606")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));
// GEMREQ-end A_22126#check

    A_22611_02.test("Access code check");
    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                      pkvTaskId.toString(),
                                      HttpStatus::Forbidden));
}

TEST_F(EndpointHandlerTest, GetChargeItemById_OldCertificate)
{
    // ERP-17321 GET ChargeItem/<id> validiert fälschlicherweise das Signaturzertifikat - B_FD-800
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50023);

    EXPECT_NO_FATAL_FAILURE(
        checkGetChargeItemByIdHandler(mServiceContext,
                                      JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757")),
                                      pkvTaskId.toString(),
                                      HttpStatus::OK,
                                      MockDatabase::mockAccessCode));
}

namespace {

// GEMREQ-start checkDeleteChargeItemHandler
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

    ASSERT_NO_FATAL_FAILURE(EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler, expectedStatus));
}
// GEMREQ-end checkDeleteChargeItemHandler

} // anonymous namespace

// GEMREQ-start A_22114
TEST_F(EndpointHandlerTest, DeleteChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const kvnr = "X500000056";
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);

    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(kvnr),
                                     pkvTaskId.toString(), HttpStatus::NoContent));

    mockDatabase.reset();
    A_22114.test("kvnr check");
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string("X999999992")),
                                     pkvTaskId.toString(), HttpStatus::Forbidden));
// GEMREQ-end A_22114

    mockDatabase.reset();
    A_22115.test("Delete chargeItem");
    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                     pkvTaskId.toString(), HttpStatus::NoContent));

    EXPECT_NO_FATAL_FAILURE(
        checkDeleteChargeItemHandler(mServiceContext,
                                     JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)),
                                     "", HttpStatus::BadRequest));
}


namespace {

// GEMREQ-start checkPatchChargeItemHandler
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPatchChargeItemHandler(
    std::optional<model::ChargeItem>& resultChargeItem,
    PcServiceContext& serviceContext,
    JWT jwt,
    const std::string_view& body,
    const model::PrescriptionId& id,
    const HttpStatus expectedStatus,
    const std::optional<std::string_view> expectedExcWhat = std::nullopt)
{
    ChargeItemPatchHandler handler({});
    Header requestHeader{
        HttpMethod::PATCH,
        "/ChargeItem/" + id.toString(),
        0,
        {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
        HttpStatus::Unknown};

    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setBody(std::string{body});
    serverRequest.setPathParameters({"id"}, {id.toString()});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler,
                                                                                    expectedStatus, expectedExcWhat));
    // GEMREQ-end checkPatchChargeItemHandler
    if (expectedStatus == HttpStatus::OK)
    {
        ASSERT_NO_THROW(resultChargeItem =
                            model::ChargeItem::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));
        ASSERT_TRUE(resultChargeItem);
        EXPECT_EQ(resultChargeItem->id(), id);
        EXPECT_EQ(resultChargeItem->prescriptionId(), id);
    }
}

} // anonymous namespace


// GEMREQ-start A_22877
TEST_F(EndpointHandlerTest, PatchChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(
        model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
        50020);
    const char* const pkvKvnr = "X500000056";
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(pkvKvnr);

    const auto& patchChargeItemBody =
            ResourceManager::instance().getStringResource(dataPath + "/charge_item_patch/Parameters_valid.json");

    std::optional<model::ChargeItem> resultChargeItem;

    ASSERT_NO_FATAL_FAILURE(checkPatchChargeItemHandler(
        resultChargeItem,
        mServiceContext,
        jwtInsurant1,
        patchChargeItemBody,
        pkvTaskId,
        HttpStatus::OK));
    EXPECT_TRUE(resultChargeItem->isMarked());

    const auto markingFlag = resultChargeItem->markingFlags();
    ASSERT_TRUE(markingFlag.has_value());
    const auto& allMarkings = markingFlag->getAllMarkings();
    EXPECT_TRUE(allMarkings.at("taxOffice"));
    EXPECT_TRUE(allMarkings.at("subsidy"));
    EXPECT_TRUE(allMarkings.at("insuranceProvider"));

    A_22877.test("kvnr check");
    ASSERT_NO_FATAL_FAILURE(checkPatchChargeItemHandler(
        resultChargeItem,
        mServiceContext,
        JwtBuilder::testBuilder().makeJwtVersicherter(std::string("X999999992")),
        patchChargeItemBody,
        pkvTaskId,
        HttpStatus::Forbidden));
// GEMREQ-end A_22877
}

namespace
{
std::vector<std::string> makeTestParameters(const fs::path& basePath, const std::string& startsWith)
{
    std::vector<std::string> params{};
    for (const auto& dirEntry : fs::directory_iterator(basePath))
    {
        if (dirEntry.is_regular_file() && String::starts_with(dirEntry.path().filename().string(), startsWith))
        {
            params.emplace_back(dirEntry.path().string());
        }
    }
    return params;
}
}

class EndpointHandlerTestPatchChargeItemInvalidParameters
    : public EndpointHandlerTest,
      public testing::WithParamInterface<std::string>
{
};

// GEMREQ-start A_22878
TEST_P(EndpointHandlerTestPatchChargeItemInvalidParameters, InvalidParameters)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(
        model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
        50020);
    const char* const pkvKvnr = "X500000056";
    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(pkvKvnr);

    const auto& patchChargeItemBody = ResourceManager::instance().getStringResource(GetParam());

    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPatchChargeItemHandler(
        resultChargeItem,
        mServiceContext,
        jwtInsurant1,
        patchChargeItemBody,
        pkvTaskId,
        HttpStatus::BadRequest, "Invalid 'Parameters' resource provided"));
    EXPECT_FALSE(resultChargeItem);
}

INSTANTIATE_TEST_SUITE_P(
    EndpointHandlerTestPatchChargeItemInvalidParametersInst, EndpointHandlerTestPatchChargeItemInvalidParameters,
    testing::ValuesIn(makeTestParameters(fs::path(TEST_DATA_DIR) / "EndpointHandlerTest" / "charge_item_patch", "Parameters_invalid_")));
// GEMREQ-end A_22878


TEST_F(EndpointHandlerTest, GetAuditEventWithInvalidEventId)//NOLINT(readability-function-cognitive-complexity)
{
    GetAuditEventHandler handler({});

    const std::string id = "9c994b00-0998-421c-9878-dc669d65ae1e";  // audit event with invalid event id (see MockDatabase)
    Header requestHeader{ HttpMethod::GET, "/AuditEvent/" + id, 0, {}, HttpStatus::Unknown};

    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X122446685");
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setPathParameters({ "id" }, { id });
    serverRequest.setAccessToken(std::move(jwt));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(callHandlerWithResponseStatusCheck(sessionContext, handler, HttpStatus::InternalServerError));
}

TEST_F(EndpointHandlerTest, GetAllTasks_DefaultSortAbsent)
{
    GetAllTasksHandler handler({});

    rapidjson::Document jwtDocument;
    std::string jwtClaims = R"({
             "professionOID": "1.2.276.0.76.4.49",
             "sub":           "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk",
             "idNummer":      "X123456788"
        })";

    jwtDocument.Parse(jwtClaims);
    auto jwt = JwtBuilder(MockCryptography::getIdpPrivateKey()).getJWT(jwtDocument);

    Header requestHeader{ HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    A_24438.test("Set explicit sort order by decreasing authored-on date");
    serverRequest.setQueryParameters({{"_sort", "-authored-on"}});
    A_24438.finish();
    serverRequest.setAccessToken(std::move(jwt));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    int h = 13;
    int m = 0;
    for (int i = 0; i < 32; ++i)
    {
        m += 3;
        if (m >= 60) {
            m = 0;
            h++;
        }
        std::ostringstream stream;
        stream << "2024-06-19T";
        stream << std::setfill('0') << std::setw(2) << h;
        stream << ":";
        stream << std::setfill('0') << std::setw(2) << m;
        stream << ":00.000+01:00";
        model::Timestamp lastUpdate = model::Timestamp::fromXsDateTime(stream.str());
        addTaskToDatabase(sessionContext, model::Task::Status::ready, "access-code", "X123456788", lastUpdate);//model::Timestamp::fromXsDateTime(xsDateTimeStr));
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
    auto tasks = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(bundle.getResourceCount(), 32 + 3); // + 3 from the default set
    A_24438.test("Ensure tasks order by decreasing authored-on date");
    ASSERT_TRUE(std::ranges::is_sorted(tasks, std::greater{}, &model::Task::authoredOn));
    A_24438.finish();
}

TEST_F(EndpointHandlerTest, GetAllTasks_DefaultSort_Success)
{
    GetAllTasksHandler handler({});

    rapidjson::Document jwtDocument;
    std::string jwtClaims = R"({
             "professionOID": "1.2.276.0.76.4.49",
             "sub":           "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk",
             "idNummer":      "X123456788"
        })";

    jwtDocument.Parse(jwtClaims);
    auto jwt = JwtBuilder(MockCryptography::getIdpPrivateKey()).getJWT(jwtDocument);

    Header requestHeader{ HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{ std::move(requestHeader) };
    serverRequest.setAccessToken(std::move(jwt));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    int h = 13;
    int m = 0;
    for (int i = 0; i < 32; ++i)
    {
        m += 3;
        if (m >= 60) {
            m = 0;
            h++;
        }
        std::ostringstream stream;
        stream << "2024-06-19T";
        stream << std::setfill('0') << std::setw(2) << h;
        stream << ":";
        stream << std::setfill('0') << std::setw(2) << m;
        stream << ":00.000+01:00";
        model::Timestamp lastUpdate = model::Timestamp::fromXsDateTime(stream.str());
        addTaskToDatabase(sessionContext, model::Task::Status::ready, "access-code", "X123456788", lastUpdate);//model::Timestamp::fromXsDateTime(xsDateTimeStr));
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
    auto tasks = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(bundle.getResourceCount(), 32 + 3); // + 3 from the default set
    A_24438.test("Ensure tasks order by increasing authored-on date");
    ASSERT_TRUE(std::ranges::is_sorted(tasks, std::less{}, &model::Task::authoredOn));
    A_24438.finish();
}


TEST_F(EndpointHandlerTest, GetAllAuditEvents_DefaultSort)
{
    const std::string gematikVersionStr{to_string(ResourceTemplates::Versions::GEM_ERP_current())};

    const auto dataPath(std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest");

    const std::string auditEventFileName = dataPath + "/audit_event_" + gematikVersionStr + ".json";
    auto auditEvent = model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(auditEventFileName));

    // Add audit events with most recent to oldest date.
    auditEvent.setId(model::Timestamp::fromXsDateTime("2024-05-28T13:34:12+01:00").toDatabaseSUuid());
    mockDatabase->insertAuditEvent(auditEvent, model::AuditEventId::POST_Task_activate);

    auditEvent.setId(model::Timestamp::fromXsDateTime("2024-05-28T13:34:11+01:00").toDatabaseSUuid());
    mockDatabase->insertAuditEvent(auditEvent, model::AuditEventId::POST_Task_activate);

    auditEvent.setId(model::Timestamp::fromXsDateTime("2024-05-28T13:34:10+02:00").toDatabaseSUuid());
    mockDatabase->insertAuditEvent(auditEvent, model::AuditEventId::POST_Task_activate);

    auditEvent.setId(model::Timestamp::fromXsDateTime("2024-05-28T13:34:09+01:00").toDatabaseSUuid());
    mockDatabase->insertAuditEvent(auditEvent, model::AuditEventId::POST_Task_activate);

    GetAllAuditEventsHandler handler({});

    Header requestHeader{ HttpMethod::GET, "/AuditEvent/", 0, { {Header::AcceptLanguage, "de"} }, HttpStatus::Unknown};
    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X122446685");
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
    ASSERT_EQ(auditEvents.size(), 4 + 1); // 4 added by this test, 1 by the MockDatabase.
    A_24438.test("Ensure audit events are received from oldest to most recent date.");
    ASSERT_TRUE(std::ranges::is_sorted(auditEvents, std::less{}, &model::AuditEvent::recorded));
    A_24438.finish();
}
