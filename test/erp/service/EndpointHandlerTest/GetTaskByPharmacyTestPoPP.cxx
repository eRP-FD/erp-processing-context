/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "EndpointHandlerTestFixture.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "shared/audit/AuditEventCreator.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/Device.hxx"
#include "test/erp/pc/popp/PoPPTokenBuilder.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"


class GetTaskByPharmacyTestPoPPP : public EndpointHandlerTest,
                                   public testing::WithParamInterface<model::PrescriptionType>
{
public:
    GetTaskByPharmacyTestPoPPP()
    {
        insertTask(GetParam(), ResourceTemplates::TaskType::Ready, 11111, model::Timestamp::now() + 48h, kvnr);
        insertTask(GetParam(), ResourceTemplates::TaskType::Ready, 21111, model::Timestamp::now() + 48h, kvnr);
    }
    void callHandler(const std::string& poppToken, const std::function<void(SessionContext& session)>& checks = {})
    {
        std::optional<ServerResponse> response;
        EXPECT_NO_THROW(response.emplace(callNoExpect(poppToken, checks)));
        ASSERT_TRUE(response.has_value());
        const model::Bundle taskBundle = model::Bundle::fromXmlNoValidation(response->getBody());
        EXPECT_NO_FATAL_FAILURE(testutils::validate(taskBundle));
        EXPECT_EQ(taskBundle.getBundleType(), model::BundleType::searchset);
        const auto tasks = taskBundle.getResourcesByType<model::Task>("Task");
        EXPECT_EQ(tasks.size(), 2);
    }

    model::Kvnr kvnr{"X123456799"};

    ServerResponse callNoExpect(const std::string& poppToken,
                                const std::function<void(SessionContext& session)>& checks = {})
    {
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
        Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
        requestHeader.addHeaderField(Header::XPoPPToken, poppToken);
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setAccessToken(jwtPharmacy);
        AccessLog accessLog;
        ServerResponse serverResponse;
        SessionContext sessionContext(mServiceContext, serverRequest, serverResponse, accessLog);

        GetAllTasksHandler handler({});
        handler.preHandleRequestHook(sessionContext);
        handler.handleRequest(sessionContext);
        if (checks)
        {
            checks(sessionContext);
        }
        return serverResponse;
    }
};

// GEMREQ-start A_22432-02#tests
TEST_P(GetTaskByPharmacyTestPoPPP, ProofMethodHealthId)
{
    A_22431_02.test("");
    A_22432_02.test("");
    auto token = PoPPTokenBuilder{}
                     .withProofMethod("healthid")
                     .withPatientId(kvnr.id())
                     .withActorId("3-SMC-B-Testkarte-883110000120312")
                     .getToken();

    auto checks = [](SessionContext& session) {
        session.auditDataCollector().fillFromAccessToken(session.request.getAccessToken());
        session.auditDataCollector().setDeviceId(model::Device::Id);
        auto auditEvent = AuditEventCreator::fromAuditData(session.auditDataCollector().createData(), "de",
                                                           AuditEventTextTemplates{}, session.request.getAccessToken());
        EXPECT_EQ(
            auditEvent.textDiv(),
            R"(<div xmlns="http://www.w3.org/1999/xhtml">Institutions- oder Organisations-Bezeichnung hat die Liste der einlösbaren E-Rezepte abgerufen durch Autorisierung mittels GesundheitsID.</div>)");
        auto auditEventEn =
            AuditEventCreator::fromAuditData(session.auditDataCollector().createData(), "en", AuditEventTextTemplates{},
                                             session.request.getAccessToken());
        EXPECT_EQ(
            auditEventEn.textDiv(),
            R"(<div xmlns="http://www.w3.org/1999/xhtml">Institutions- oder Organisations-Bezeichnung retrieved a list of prescriptions to be redeemed by authorization with GesundheitsID.</div>)");
        EXPECT_EQ(session.getBdeUseCase(), bde::GetTasksPoPP_UC_4_15);
    };
    callHandler(token.serialize(), checks);
}
TEST_P(GetTaskByPharmacyTestPoPPP, ProofMethodEhcPractitioner)
{
    A_22431_02.test("");
    A_22432_02.test("");
    auto token = PoPPTokenBuilder{}
                     .withProofMethod("ehc-practitioner-cvc-authenticated")
                     .withPatientId(kvnr.id())
                     .withActorId("3-SMC-B-Testkarte-883110000120312")
                     .getToken();

    auto checks = [](SessionContext& session) {
        session.auditDataCollector().fillFromAccessToken(session.request.getAccessToken());
        session.auditDataCollector().setDeviceId(model::Device::Id);
        auto auditEvent = AuditEventCreator::fromAuditData(session.auditDataCollector().createData(), "de",
                                                           AuditEventTextTemplates{}, session.request.getAccessToken());
        EXPECT_EQ(
            auditEvent.textDiv(),
            R"(<div xmlns="http://www.w3.org/1999/xhtml">Institutions- oder Organisations-Bezeichnung hat die Liste der einlösbaren E-Rezepte abgerufen durch Autorisierung mittels Gesundheitskarte in der Apotheke.</div>)");
        EXPECT_EQ(session.getBdeUseCase(), bde::GetTasksPoPP_UC_4_15);
    };
    callHandler(token.serialize(), checks);
}
TEST_P(GetTaskByPharmacyTestPoPPP, ProofMethodEhcProvider)
{
    A_22431_02.test("");
    A_22432_02.test("");
    auto token = PoPPTokenBuilder{}
                     .withProofMethod("ehc-provider-user-x509")
                     .withPatientId(kvnr.id())
                     .withActorId("3-SMC-B-Testkarte-883110000120312")
                     .getToken();

    auto checks = [](SessionContext& session) {
        session.auditDataCollector().fillFromAccessToken(session.request.getAccessToken());
        session.auditDataCollector().setDeviceId(model::Device::Id);
        auto auditEvent = AuditEventCreator::fromAuditData(session.auditDataCollector().createData(), "de",
                                                           AuditEventTextTemplates{}, session.request.getAccessToken());
        EXPECT_EQ(
            auditEvent.textDiv(),
            R"(<div xmlns="http://www.w3.org/1999/xhtml">Institutions- oder Organisations-Bezeichnung hat die Liste der einlösbaren E-Rezepte abgerufen durch Autorisierung mittels Gesundheitskarte über ein Endgerät des Versicherten.</div>)");
        EXPECT_EQ(session.getBdeUseCase(), bde::GetTasksPoPP_UC_4_15);
    };
    callHandler(token.serialize(), checks);
}

TEST_P(GetTaskByPharmacyTestPoPPP, ProofMethodUnknown)
{
    A_22431_02.test("");
    A_22432_02.test("");
    auto token = PoPPTokenBuilder{}
    .withProofMethod("ehc-other-x509")
    .withPatientId(kvnr.id())
    .withActorId("3-SMC-B-Testkarte-883110000120312")
    .getToken();

    auto checks = [](SessionContext& session) {
        session.auditDataCollector().fillFromAccessToken(session.request.getAccessToken());
        session.auditDataCollector().setDeviceId(model::Device::Id);
        auto auditEvent = AuditEventCreator::fromAuditData(session.auditDataCollector().createData(), "de",
                                                           AuditEventTextTemplates{}, session.request.getAccessToken());
        EXPECT_EQ(
            auditEvent.textDiv(),
            R"(<div xmlns="http://www.w3.org/1999/xhtml">Institutions- oder Organisations-Bezeichnung hat die Liste der einlösbaren E-Rezepte abgerufen durch Autorisierung mittels eines Nachweises des Versorgungskontextes.</div>)");
        EXPECT_EQ(session.getBdeUseCase(), bde::GetTasksPoPP_UC_4_15);
    };
    callHandler(token.serialize(), checks);
}


TEST_P(GetTaskByPharmacyTestPoPPP, MissingPoPPToken)
{
    A_22432_02.test("Missing PoPP Token");
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callNoExpect(""), HttpStatus::Forbidden, "POPP-Token verification error.",
                                          "Pre-verification failed - expecting JWS Compact Serialization.");
}
TEST_P(GetTaskByPharmacyTestPoPPP, WrongPoPPTokenSignature)
{
    A_22432_02.test("Wrong PoPP Token signature");
    const SafeString prvPem{std::string{ResourceManager::instance().getStringResource(
        "test/generated_pki/sub_ca1_ec/certificates/qes_cert1_ec/qes_cert1_ec_key.pem")}};
    auto wrongKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(prvPem);
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callNoExpect(PoPPTokenBuilder{}.getToken(wrongKey).serialize()),
                                          HttpStatus::Forbidden, "POPP-Token verification error.",
                                          "Verification failed - invalid signature or payload.");
}

TEST_P(GetTaskByPharmacyTestPoPPP, IATTooOld)
{
    A_23399_01.test("IAT older than 30 min");
    using namespace std::chrono_literals;
    auto token = PoPPTokenBuilder{}.withIat((model::Timestamp::now() - 31min).toTimeT()).getToken();
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callNoExpect(token.serialize()), HttpStatus::Forbidden,
                                          "POPP-Token verification error.", "PoPPToken.iat is older than 30 minutes");
}

TEST_P(GetTaskByPharmacyTestPoPPP, TaskNotReady)
{
    A_22431_02.test("Task not ready");
    insertTask(GetParam(), ResourceTemplates::TaskType::Draft, 31111, model::Timestamp::now() + 48h, kvnr);
    insertTask(GetParam(), ResourceTemplates::TaskType::InProgress, 41111, model::Timestamp::now() + 48h, kvnr);
    insertTask(GetParam(), ResourceTemplates::TaskType::Completed, 51111, model::Timestamp::now() + 48h, kvnr);
    auto token =
        PoPPTokenBuilder{}.withPatientId(kvnr.id()).withActorId("3-SMC-B-Testkarte-883110000120312").getToken();
    callHandler(token.serialize());
}

TEST_P(GetTaskByPharmacyTestPoPPP, TaskNotKvnr)
{
    A_22431_02.test("Task.for != KVNR");
    insertTask(GetParam(), ResourceTemplates::TaskType::Ready, 31111, model::Timestamp::now() + 48h,
               model::Kvnr{"X123456798"});
    auto token =
        PoPPTokenBuilder{}.withPatientId(kvnr.id()).withActorId("3-SMC-B-Testkarte-883110000120312").getToken();
    callHandler(token.serialize());
}

TEST_P(GetTaskByPharmacyTestPoPPP, TaskExpired)
{
    A_22431_02.test("Task expired");
    insertTask(GetParam(), ResourceTemplates::TaskType::Ready, 31111, model::Timestamp::now() - 48h, kvnr);
    auto token =
        PoPPTokenBuilder{}.withPatientId(kvnr.id()).withActorId("3-SMC-B-Testkarte-883110000120312").getToken();
    callHandler(token.serialize());
}

TEST_P(GetTaskByPharmacyTestPoPPP, TaskWrongFlowtype)
{
    A_22431_02.test("Task:flowtype != 160,166");
    insertTask(model::PrescriptionType::digitaleGesundheitsanwendungen, ResourceTemplates::TaskType::Ready, 31111,
               model::Timestamp::now() + 48h, kvnr);
    insertTask(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, ResourceTemplates::TaskType::Ready, 41111,
               model::Timestamp::now() + 48h, kvnr);
    insertTask(model::PrescriptionType::direkteZuweisung, ResourceTemplates::TaskType::Ready, 61111,
               model::Timestamp::now() + 48h, kvnr);
    insertTask(model::PrescriptionType::direkteZuweisungPkv, ResourceTemplates::TaskType::Ready, 71111,
               model::Timestamp::now() + 48h, kvnr);
    auto token =
        PoPPTokenBuilder{}.withPatientId(kvnr.id()).withActorId("3-SMC-B-Testkarte-883110000120312").getToken();
    callHandler(token.serialize());
}

TEST_P(GetTaskByPharmacyTestPoPPP, PoPPTokenWrongActorId)
{
    A_23402_01.test("PoPPToken.actorId != ACCESS_TOKEN.idNumber");
    auto poPPToken = PoPPTokenBuilder{}.withActorId("1-23456789").getToken();
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callNoExpect(poPPToken.serialize()), HttpStatus::Forbidden,
                                          "POPP-Token verification error.",
                                          "PoPPToken.actorId does not match ACCESS_TOKEN.idNumber");
}


INSTANTIATE_TEST_SUITE_P(PoPP, GetTaskByPharmacyTestPoPPP,
                         testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                         model::PrescriptionType::tRezept));
// GEMREQ-end A_22432-02#tests
