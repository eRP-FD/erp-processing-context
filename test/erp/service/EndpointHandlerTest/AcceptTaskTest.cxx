/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/Consent.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/util/ByteHelper.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <erp/model/OperationOutcome.hxx>

class AcceptTaskTest : public EndpointHandlerTest
{
};

namespace
{

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkAcceptTaskSuccessCommon(std::optional<model::Bundle>& resultBundle, PcServiceContext& serviceContext,
                                  const std::string_view& taskJson, const std::string_view& kbvBundleXml,
                                  unsigned int numOfExpectedResources)
{
    const auto& task = model::Task::fromJsonNoValidation(taskJson);

    AcceptTaskHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + task.prescriptionId().toString() + "/$accept/",
                         0,
                         {},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {task.prescriptionId().toString()});
    serverRequest.setQueryParameters({{"ac", std::string(task.accessCode())}});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());

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
    ASSERT_NO_THROW(resultBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
    ASSERT_EQ(resultBundle->getResourceCount(), numOfExpectedResources);

    const auto tasks = resultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    EXPECT_EQ(tasks[0].prescriptionId(), task.prescriptionId());

    A_19169_01.test("Check task status, secret, QES bundle");
    EXPECT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    EXPECT_TRUE(tasks[0].secret().has_value());
    EXPECT_NO_FATAL_FAILURE((void)ByteHelper::fromHex(*tasks[0].secret()));

    // GEMREQ-start A_24174#test2
    A_24174.test("owner has been stored");
    EXPECT_TRUE(tasks[0].owner().has_value());
    EXPECT_EQ(tasks[0].owner(), serverRequest.getAccessToken().stringForClaim(JWT::idNumberClaim));
    // GEMREQ-end A_24174#test2

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

}// anonymous namespace

TEST_F(AcceptTaskTest, AcceptTaskSuccess)
{
    auto kbvBundle = ResourceTemplates::kbvBundleXml();
    std::optional<model::Bundle> resultBundle;
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714);
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId}),
        kbvBundle, 2 /*numOfExpectedResources*/));
}

// Regression test for ERP-10628
TEST_F(AcceptTaskTest, AcceptTaskSuccessNoQesCheck)
{
    auto factories = StaticData::makeMockFactories();
    auto kbvBundle = ResourceTemplates::kbvBundleXml();

    // create a TSL Manager that generates failures.
    factories.tslManagerFactory = [](const std::shared_ptr<XmlValidator>&) {
        auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
        X509Certificate certX509 = X509Certificate::createFromBase64(cert.toBase64Der());
        auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
        auto privKey =
            EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});
        const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());

        // trigger revoked status in OCSP-Responder for the certificate
        std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
            {}, {}, {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::REVOKED}}}});
        return tslManager;
    };
    factories.databaseFactory = [](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
        auto md = std::make_unique<MockDatabase>(hsmPool);
        md->fillWithStaticData();
        return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
    };

    PcServiceContext serviceContext(Configuration::instance(), std::move(factories));
    std::optional<model::Bundle> resultBundle;
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714);
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, serviceContext,
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId}),
        kbvBundle, 2 /*numOfExpectedResources*/));
}

TEST_F(AcceptTaskTest, AcceptTaskInvalidMvoDate)
{
    // this tests is required for the case when we accepted MVOs with invalid
    // dates, i.e. dates that where parsable by Timestamp::fromFhirDateTime().
    // Even though we dont allow these prescriptions at accept task anymore,
    // they are already persisted and in order to allow compatibility with
    // older prescriptions, we must allow these dates at the accept operation
    // Create Task in database
    auto db = mServiceContext.databaseFactory();
    auto task = model::Task::fromJsonNoValidation(
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft}));
    const auto taskId = db->storeTask(task);
    task.setPrescriptionId(taskId);
    task.setHealthCarePrescriptionUuid();
    task.setKvnr(model::Kvnr{"X234567891", model::Kvnr::Type::gkv});
    task.setAcceptDate(model::Timestamp::fromGermanDate("2022-04-02"));
    using namespace std::chrono_literals;
    const auto tomorrow = model::Timestamp(std::chrono::system_clock::now() + 24h);
    task.setExpiryDate(tomorrow);
    auto kbvBundle =
        ResourceTemplates::kbvBundleMvoXml({.prescriptionId = taskId, .redeemPeriodEnd = tomorrow.toXsDateTime()});

    const auto healthCarePrescriptionUuid = task.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle =
        model::Binary(healthCarePrescriptionUuid, CryptoHelper::toCadesBesSignature(kbvBundle));
    task.setStatus(model::Task::Status::ready);
    db->activateTask(task, healthCarePrescriptionBundle);
    db->commitTransaction();
    db.reset();

    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(resultBundle, mServiceContext, task.serializeToJsonString(),
                                                         kbvBundle, 2 /*numOfExpectedResources*/));
}

TEST_F(AcceptTaskTest, AcceptTaskPkvWithConsent)//NOLINT(readability-function-cognitive-complexity)
{
    if (model::ResourceVersion::deprecatedProfile(
            model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
    {
        GTEST_SKIP();
    }

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const char* const pkvKvnr = "X500000056";

    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
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

TEST_F(AcceptTaskTest, AcceptTaskPkvWithoutConsent)
{
    if (model::ResourceVersion::deprecatedProfile(
            model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
    {
        GTEST_SKIP();
    }

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50001);
    const char* const pkvKvnr = "X500000017";

    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        2 /*numOfExpectedResources*/));// no consent resource in result;
}

TEST_F(AcceptTaskTest, AcceptTaskPkvWithConsent209)//NOLINT(readability-function-cognitive-complexity)
{
    if (model::ResourceVersion::deprecatedProfile(
            model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
    {
        GTEST_SKIP();
    }

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50002);
    const char* const pkvKvnr = "X500000029";

    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
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

TEST_F(AcceptTaskTest, AcceptTaskPkvWithoutConsent209)
{
    if (model::ResourceVersion::deprecatedProfile(
            model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
    {
        GTEST_SKIP();
    }

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50003);
    const char* const pkvKvnr = "X500000031";

    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .kvnr = pkvKvnr}),
        2 /*numOfExpectedResources*/));// no consent resource in result;
}

TEST_F(AcceptTaskTest, AcceptTaskFail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    AcceptTaskHandler handler({});
    {
        // Unknown Task;
        const auto id =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 999999)
                .toString();
        Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);

        serverRequest.setPathParameters({"id"}, {"9a27d600-5a50-4e2b-98d3-5e05d2e85aa0"});
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound)
    }

    const auto id =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
    {
        // No Access Code;
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
    {
        // Invalid Access Code;
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setQueryParameters({{"ac", "invalid_access_code"}});
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
    {
        // No id;
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
}

TEST_F(AcceptTaskTest, AcceptTaskAlreadyInProgress)
{
    A_19168_01.test("AcceptTaskAlreadyInProgress");
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    AcceptTaskHandler handler({});
    const auto id =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{Header(requestHeader)};
    serverRequest.setPathParameters({"id"}, {id});
    serverRequest.setQueryParameters({{"ac", validAccessCode}});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
    {
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    }
    {
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke("different-telematik-id"));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::Conflict,
                                          "Task has invalid status in-progress");
    }
}

TEST_F(AcceptTaskTest, AcceptTaskAlreadyInProgressSelf)
{
    A_19168_01.test("AcceptTaskAlreadyInProgressSelf");
    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    AcceptTaskHandler handler({});
    const auto id =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();
    const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{Header(requestHeader)};
    serverRequest.setPathParameters({"id"}, {id});
    serverRequest.setQueryParameters({{"ac", validAccessCode}});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
    {
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    }
    {
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        try
        {
            handler.handleRequest(sessionContext);
        }
        catch (const ErpServiceException& serviceException)
        {
            ASSERT_EQ(serviceException.status(), HttpStatus::Conflict);
            const auto issues = serviceException.operationOutcome().issues();
            ASSERT_EQ(issues.size(), 2);
            EXPECT_EQ(issues[0].code, model::OperationOutcome::Issue::Type::conflict);
            EXPECT_FALSE(issues[0].diagnostics.has_value());
            EXPECT_TRUE(issues[0].expression.empty());
            EXPECT_EQ(issues[0].severity, model::OperationOutcome::Issue::Severity::error);
            EXPECT_EQ(issues[0].detailsText, "Task has invalid status in-progress");
            EXPECT_EQ(issues[1].code, model::OperationOutcome::Issue::Type::conflict);
            EXPECT_FALSE(issues[1].diagnostics.has_value());
            EXPECT_TRUE(issues[1].expression.empty());
            EXPECT_EQ(issues[1].severity, model::OperationOutcome::Issue::Severity::error);
            EXPECT_EQ(issues[1].detailsText, "Task is processed by requesting institution");
        }
    }
}

TEST_F(AcceptTaskTest, AcceptTaskFailExpiryDate)
{
    auto kbvBundle = ResourceTemplates::kbvBundleXml();

    using namespace std::chrono_literals;
    const auto yesterday = model::Timestamp::now() - 24h;
    const auto taskId = model::PrescriptionId::fromDatabaseId(
        model::PrescriptionType::apothekenpflichigeArzneimittel, 11508);
    const auto taskJson = ResourceTemplates::taskJson({
        .taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId, .expirydate = yesterday});
    model::Task task = model::Task::fromJsonNoValidation(taskJson);
    mockDatabase->insertTask(task);

    try
    {
        const auto id = taskId.toString();
        const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
        AcceptTaskHandler handler({});
        const Header requestHeader{HttpMethod::POST, "/Task/" + id + "/$accept/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {id});
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        handler.handleRequest(sessionContext);
    }
    catch(const ErpException& erpException)
    {
        EXPECT_EQ(erpException.what(), std::string("Verordnung bis " + yesterday.toGermanDateFormat() + " einlösbar."));
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "expected ErpException but got: " << typeid(ex).name() << ": " << ex.what();
    }
}
