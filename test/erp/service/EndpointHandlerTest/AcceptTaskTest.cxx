/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/Consent.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

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

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    // TODO: ERP-10782: re-enable validation for WF 209
//    ASSERT_NO_THROW(resultBundle = model::Bundle::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
//                                                          *StaticData::getInCodeValidator(), SchemaType::fhir));
    ASSERT_NO_THROW(resultBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
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

}// anonymous namespace

TEST_F(AcceptTaskTest, AcceptTaskSuccess)
{
    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext, resourceManager.getStringResource(dataPath + "/task4.json"),
        resourceManager.getStringResource(dataPath + "/kbv_bundle.xml"), 2 /*numOfExpectedResources*/));
}

// Regression test for ERP-10628
TEST_F(AcceptTaskTest, AcceptTaskSuccessNoQesCheck)
{
    auto& resourceManager = ResourceManager::instance();
    auto factories = StaticData::makeMockFactories();

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
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, serviceContext, resourceManager.getStringResource(dataPath + "/task4.json"),
        resourceManager.getStringResource(dataPath + "/kbv_bundle.xml"), 2 /*numOfExpectedResources*/));
}

TEST_F(AcceptTaskTest, AcceptTaskPkvWithConsent)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const char* const pkvKvnr = "X500000000";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
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
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50001);
    const char* const pkvKvnr = "X500000001";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
        2 /*numOfExpectedResources*/));// no consent resource in result;
}

TEST_F(AcceptTaskTest, AcceptTaskPkvWithConsent209)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50002);
    const char* const pkvKvnr = "X500000002";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
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
    EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50003);
    const char* const pkvKvnr = "X500000003";

    auto& resourceManager = ResourceManager::instance();
    std::optional<model::Bundle> resultBundle;
    ASSERT_NO_FATAL_FAILURE(checkAcceptTaskSuccessCommon(
        resultBundle, mServiceContext,
        replaceKvnr(
            replacePrescriptionId(resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json"),
                                  pkvTaskId.toString()),
            pkvKvnr),
        replaceKvnr(replacePrescriptionId(resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml"),
                                          pkvTaskId.toString()),
                    pkvKvnr),
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