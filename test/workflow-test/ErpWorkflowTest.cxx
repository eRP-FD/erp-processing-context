/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/model/OuterResponseErrorData.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/fhir/Fhir.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

#include <thread>
#include <date/tz.h>


TEST_F(ErpWorkflowTest, UserPseudonym) // NOLINT
{
    ClientResponse outerResponse;
    ClientResponse innerResponse;
    JWT jwt = defaultJwt();
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, innerResponse) = send(RequestArguments{HttpMethod::GET, "/Task/", {}}
        .withJwt(jwt)
        .withHeader( Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    if (outerResponse.getHeader().hasHeader(std::string{ProxyUserPseudonymHeader}))
    {
        auto proxyUP = outerResponse.getHeader().header(std::string{ProxyUserPseudonymHeader}).value();
        auto splitUp = String::split(proxyUP, '-');
        ASSERT_EQ(splitUp.size(), 2);
        EXPECT_EQ(splitUp[0].size(), 32);
        EXPECT_EQ(splitUp[1].size(), 32);
        userPseudonym = proxyUP;
        userPseudonymType = UserPseudonymType::UserPseudonym;
    }
    else if (outerResponse.getHeader().hasHeader(std::string{VauPreUserPseudonymHeader}))
    {
        auto vauPUP = outerResponse.getHeader().header(std::string{VauPreUserPseudonymHeader}).value();
        EXPECT_EQ(vauPUP.size(), 32);
        userPseudonym = vauPUP;
        userPseudonymType = UserPseudonymType::PreUserPseudonym;
    }
    ASSERT_NE(userPseudonymType, UserPseudonymType::None) << "No (Pre-)User-Pseudonym in response";

    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, innerResponse) = send(RequestArguments{HttpMethod::GET, "/Task/", {}}
        .withJwt(jwt)
        .withHeader( Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    switch(userPseudonymType)
    {
        using UPT = UserPseudonymType;
        case UPT::PreUserPseudonym:
        {
            ASSERT_TRUE(outerResponse.getHeader().hasHeader(std::string{VauPreUserPseudonymHeader}));
            EXPECT_EQ(outerResponse.getHeader().header(std::string{VauPreUserPseudonymHeader}).value(), userPseudonym);
            break;
        }
        case UPT::UserPseudonym:
        {
            ASSERT_TRUE(outerResponse.getHeader().hasHeader(std::string{ProxyUserPseudonymHeader}));
            EXPECT_EQ(outerResponse.getHeader().header(std::string{ProxyUserPseudonymHeader}).value(), userPseudonym);
            break;
        }
        case UPT::None:
        default:
            FAIL();
    }
}

TEST_F(ErpWorkflowTest, ActivateTaskUnsupportedProfile)
{
    if (runsInCloudEnv())
    {
        GTEST_SKIP_("skipped in cloud environment");
    }
    std::optional<model::Task> task;
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    const auto signingTime = model::Timestamp::now();
    // create a task with the deprecated profile and send them to a server with the new profile set
    {
        auto envVars = testutils::getOldFhirProfileEnvironment();
        ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
        ASSERT_TRUE(task.has_value());

        auto kbvBundle = kbvBundleXml({.authoredOn = signingTime});
        auto envVarsNew = testutils::getNewFhirProfileEnvironment();
        ASSERT_NO_FATAL_FAILURE(result =
                                    taskActivate(task->prescriptionId(), task->accessCode(),
                                                 toCadesBesSignature(kbvBundle, signingTime), HttpStatus::BadRequest));
        ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
        const model::OperationOutcome& operationOutcome = std::get<model::OperationOutcome>(*result);
        ASSERT_TRUE(String::contains(operationOutcome.issues().at(0).detailsText.value_or(""),
                                 "Unsupported profile"));
    }
}


TEST_P(ErpWorkflowTestP, MultipleTaskCloseError)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    // Activate.
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    // invoke /task/<id>/$accept
    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));

    const auto telematicId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematicId.has_value());
    const JWT jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtInsurant));
    ASSERT_TRUE(communicationsBundle);
    EXPECT_EQ(countTaskBasedCommunications(*communicationsBundle, *prescriptionId), communications.size());
    auto medicationDipenseRenderVersion =
        model::ResourceVersion::fhirProfileBundleFromSchemaVersion(serverGematikProfileVersion());
    const auto closeBody = medicationDispense(kvnr, prescriptionId->toString(), model::Timestamp::now().toGermanDate(),
                                              medicationDipenseRenderVersion);
    const std::string closePath = "/Task/" + prescriptionId->toString() + "/$close?secret=" + secret;
    const JWT jwt{ jwtApotheke() };
    ClientResponse serverResponse;

    // Test that first close request succeeds.
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, closePath, closeBody, "application/fhir+xml"}
                .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK) << serverResponse.getBody();

    // Test that any further close request is denied.
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, closePath, closeBody, "application/fhir+xml"}
                .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Forbidden);
}

// GEMREQ-start A_19169-01#TaskLifecycleNormal
TEST_P(ErpWorkflowTestP, TaskLifecycleNormal)// NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    // invoke /task/<id>/$accept
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));
// GEMREQ-end A_19169-01#TaskLifecycleNormal

    // Close task
    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, *lastModifiedDate, communications));

    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGet(kvnr));
    {
        ASSERT_TRUE(taskBundle);
        std::optional<model::Task> task;
        for (auto&& t : taskBundle->getResourcesByType<model::Task>("Task"))
        {
            ASSERT_TRUE(t.kvnr().has_value());
            EXPECT_EQ(t.kvnr().value(), kvnr);
            ASSERT_NO_FATAL_FAILURE(checkTask(t.jsonDocument()););
            if (t.prescriptionId().toDatabaseId() == prescriptionId->toDatabaseId())
            {
                task = std::move(t);
                break;
            }
        }
        ASSERT_TRUE(task);
        EXPECT_ANY_THROW((void)task->accessCode());
        EXPECT_FALSE(task->secret().has_value());
        EXPECT_FALSE(task->owner().has_value());
    }

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { prescriptionId }, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr }, { 0, 2, 3, 4, 5 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read });
}

TEST_P(ErpWorkflowTestP, TaskLifecycleAbortByInsurantProxy) // NOLINT
{
    if (isUnsupportedFlowtype(GetParam()) || GetParam() == model::PrescriptionType::direkteZuweisung ||
        GetParam() ==
            model::PrescriptionType::direkteZuweisungPkv)// Abort of activated task not allowed for "direkteZuweisung"
    {
        GTEST_SKIP();
    }

    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    const auto kvnr = generateNewRandomKVNR();
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr.id(), accessCode));

    std::optional<model::Kvnr> kvnrRepresentative;
    for (const auto& communication : communications)
    {
        if (communication.messageType() == model::Communication::MessageType::Representative &&
            communication.sender().has_value() && model::getIdentityString(communication.sender().value()) != kvnr)
        {
            kvnrRepresentative = std::get<model::Kvnr>(communication.recipient());
            break;
        }
    }
    ASSERT_TRUE(kvnrRepresentative.has_value());
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId,
                                           JwtBuilder::testBuilder().makeJwtVersicherter(kvnrRepresentative->id()),
                                           kvnr.id(), {accessCode}, {}, communications));

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { prescriptionId }, kvnr.id(), "en", startTime,
        { telematicIdDoctor, kvnr.id(), kvnrRepresentative->id() }, { 0 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::del});
}

TEST_P(ErpWorkflowTestP, TaskLifecycleAbortByInsurant) // NOLINT
{
    if (isUnsupportedFlowtype(GetParam()) || GetParam() == model::PrescriptionType::direkteZuweisung ||
        GetParam() ==
            model::PrescriptionType::direkteZuweisungPkv)// Abort of activated task not allowed for "direkteZuweisung"
    {
        GTEST_SKIP();
    }

    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    // No access code -> KV-Nr check:
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), kvnr, { }, { }, communications));

    // Check for correct return code in this case, for bugticket ERP-4972
    taskAccept(*prescriptionId,accessCode, HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing);

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { prescriptionId }, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, kvnr }, { 0 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::del });
}

TEST_P(ErpWorkflowTestP, TaskLifecycleAbortByPharmacy) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));

    // Pharmacy -> check of secret:
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId, jwtApotheke(), kvnr, { }, { secret }, communications));

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { prescriptionId }, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy }, { 0, 2, 3, 4 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::del });
}

TEST_P(ErpWorkflowTestP, TaskLifecycleReject) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));

    // Reject Task
    ASSERT_NO_FATAL_FAILURE(checkTaskReject(*prescriptionId, kvnr, accessCode, secret));

    // May be accepted again
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));

    // Close task
    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, *lastModifiedDate, communications));

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { prescriptionId }, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr,
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr}, { 0, 2, 3, 4, 6, 7, 8, 9 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::read });
}

TEST_P(ErpWorkflowTestP, TaskSearchStatus) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3, GetParam()));

    std::string qesBundle1;
    std::string qesBundle2;
    std::string qesBundle3;
    std::vector<model::Communication> communications1;
    std::vector<model::Communication> communications2;
    std::vector<model::Communication> communications3;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle3, communications3, *prescriptionId3, kvnr, accessCode3));

    {
        // Case from bug ticket (ERP-6043). Operation "eq" is not supported for "status", therefore this is treated
        // as unknown value for status and should cause a BadRequest error. It formerly caused an internal server error.
        ASSERT_NO_FATAL_FAILURE(taskGet(kvnr, "modified=eq2021-06-18&status=eqREADY",
                                        HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
    }
    {
        const auto bundle = taskGet(kvnr, "status=ready").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 3);
    }
    {
        const auto bundle = taskGet(kvnr, "status=in-progress").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }
    {
        const auto bundle = taskGet(kvnr, "status=completed").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }
    {
        const auto bundle = taskGet(kvnr, "status=cancelled").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }

    std::string secret1;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret1, lastModifiedDate, *prescriptionId1, kvnr, accessCode1, qesBundle1));

    {
        const auto bundle = taskGet(kvnr, "status=ready").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 2);
    }
    {
        const auto bundle = taskGet(kvnr, "status=in-progress").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 1);
    }
    {
        const auto bundle = taskGet(kvnr, "status=completed").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }
    {
        const auto bundle = taskGet(kvnr, "status=cancelled").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }

    std::string secret2;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret2, lastModifiedDate, *prescriptionId2, kvnr, accessCode2, qesBundle2));
    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId2, kvnr, secret2, *lastModifiedDate, communications2));
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId3, jwtArzt(), kvnr, accessCode3, {}, communications3));

    {
        const auto bundle = taskGet(kvnr, "status=ready").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
    }
    {
        const auto bundle = taskGet(kvnr, "status=in-progress").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 1);
    }
    {
        const auto bundle = taskGet(kvnr, "status=completed").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 1);
    }
    {
        const auto bundle = taskGet(kvnr, "status=cancelled").value();
        EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), 1);
    }
}

TEST_F(ErpWorkflowTest, TaskSearchStatusERP4627) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    RequestArguments args(HttpMethod::GET, "/Task?modified=; select 1 from dual;", {});
    args.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    args.overrideExpectedInnerOperation = "UNKNOWN";
    args.overrideExpectedInnerRole = "XXX";
    args.overrideExpectedInnerClientId = "XXX";
    ClientResponse outerResponse;
    ClientResponse innerResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, innerResponse) = send(args));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);

    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentType));
    // Default result format is XML, if no inner request could be created:
    ASSERT_EQ(innerResponse.getHeader().header(Header::ContentType).value(), "application/fhir+xml;charset=utf-8");
    checkOperationOutcome(operationOutcomeFromResponse(innerResponse.getBody(), false /*Json*/),
                          model::OperationOutcome::Issue::Type::invalid, "Parsing the HTTP message header failed.");
}

TEST_P(ErpWorkflowTestP, TaskSearchLastModified) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3, GetParam()));

    std::string qesBundle1;
    std::string qesBundle2;
    std::string qesBundle3;
    std::vector<model::Communication> communications1;
    std::vector<model::Communication> communications2;
    std::vector<model::Communication> communications3;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle3, communications3, *prescriptionId3, kvnr, accessCode3));

    std::string secret1;
    std::string secret2;
    std::string secret3;
    std::optional<model::Timestamp> lastModifiedDate1;
    std::optional<model::Timestamp> lastModifiedDate2;
    std::optional<model::Timestamp> lastModifiedDate3;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret1, lastModifiedDate1, *prescriptionId1, kvnr, accessCode1, qesBundle1));
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret2, lastModifiedDate2, *prescriptionId2, kvnr, accessCode2, qesBundle2));
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret3, lastModifiedDate3, *prescriptionId3, kvnr, accessCode3, qesBundle3));

    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("modified=ge" +
                                       lastModifiedDate1->toXsDateTimeWithoutFractionalSeconds()));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 3);
    }

    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("modified=lt" +
                                       lastModifiedDate1->toXsDateTimeWithoutFractionalSeconds()));
        EXPECT_EQ(bundle->getResourceCount(), 0);
    }
}

TEST_P(ErpWorkflowTestP, TaskSearchAuthoredOn ) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));
    std::this_thread::sleep_for(1s);
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3, GetParam()));

    std::string qesBundle1;
    std::string qesBundle2;
    std::string qesBundle3;
    std::vector<model::Communication> communications1;
    std::vector<model::Communication> communications2;
    std::vector<model::Communication> communications3;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle3, communications3, *prescriptionId3, kvnr, accessCode3));

    const auto task1 = std::move(taskGetId(*prescriptionId1, kvnr, accessCode1)->getResourcesByType<model::Task>("Task").at(0));
    const auto task2 = std::move(taskGetId(*prescriptionId2, kvnr, accessCode2)->getResourcesByType<model::Task>("Task").at(0));
    const auto task3 = std::move(taskGetId(*prescriptionId3, kvnr, accessCode3)->getResourcesByType<model::Task>("Task").at(0));

    const auto dateTime1 = task1.authoredOn().toXsDateTimeWithoutFractionalSeconds(); // returns UTC;
    const auto dateTime2 = task2.authoredOn().toXsDateTimeWithoutFractionalSeconds();

    // without a given timezone, the times will be interpreted as German TZ
    const auto dateTimeWithoutTz1 =
        task1.authoredOn().toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19);
    const auto dateTimeWithoutTz2 =
        task2.authoredOn().toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19);

    {
        auto bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=ge" + dateTime1));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 3);
        bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=ge" + dateTimeWithoutTz1));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 3);
    }

    {
        auto bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=ge" + dateTime2));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 2);
        bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=ge" + dateTimeWithoutTz2));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 2);
    }

    {
        auto bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=lt" + dateTime2));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);
        bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=lt" + dateTimeWithoutTz2));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);
    }

    {
        auto bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=" + dateTime1));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);
        bundle = taskGet(kvnr, UrlHelper::escapeUrl("authored-on=" + dateTimeWithoutTz1));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);
    }
}

TEST_P(ErpWorkflowTestP, TaskGetPaging ) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3, GetParam()));

    std::string qesBundle1;
    std::string qesBundle2;
    std::string qesBundle3;
    std::vector<model::Communication> communications1;
    std::vector<model::Communication> communications2;
    std::vector<model::Communication> communications3;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle3, communications3, *prescriptionId3, kvnr, accessCode3));

    const auto totalSearchMatches = 3;

    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("_count=2"));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 2);
        EXPECT_EQ(bundle->getTotalSearchMatches(), totalSearchMatches);
    }
    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("_count=2&__offset=2"));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);
        EXPECT_EQ(bundle->getTotalSearchMatches(), totalSearchMatches);
    }
    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("_count=2&__offset=4"));
        EXPECT_EQ(bundle->getResourceCount(), 0);
        EXPECT_EQ(bundle->getTotalSearchMatches(), totalSearchMatches);
    }
}

TEST_P(ErpWorkflowTestP, TaskGetSorting ) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3, GetParam()));

    std::string qesBundle1;
    std::string qesBundle2;
    std::string qesBundle3;
    std::vector<model::Communication> communications1;
    std::vector<model::Communication> communications2;
    std::vector<model::Communication> communications3;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications1, *prescriptionId2, kvnr, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications2, *prescriptionId1, kvnr, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle3, communications3, *prescriptionId3, kvnr, accessCode3));

    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("_sort=modified"));
        auto tasks = bundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasks.size(), 3);

        // should be the order of the activation.
        EXPECT_EQ(tasks.at(0).prescriptionId().toDatabaseId(), prescriptionId2->toDatabaseId());
        EXPECT_EQ(tasks.at(1).prescriptionId().toDatabaseId(), prescriptionId1->toDatabaseId());
        EXPECT_EQ(tasks.at(2).prescriptionId().toDatabaseId(), prescriptionId3->toDatabaseId());
    }
    {
        const auto bundle = taskGet(
            kvnr, UrlHelper::escapeUrl("_sort=-modified"));
        auto tasks = bundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasks.size(), 3);

        // should be the reverse order of the activation.
        EXPECT_EQ(tasks.at(0).prescriptionId().toDatabaseId(), prescriptionId3->toDatabaseId());
        EXPECT_EQ(tasks.at(1).prescriptionId().toDatabaseId(), prescriptionId1->toDatabaseId());
        EXPECT_EQ(tasks.at(2).prescriptionId().toDatabaseId(), prescriptionId2->toDatabaseId());
    }
}

TEST_P(ErpWorkflowTestP, TaskGetPagingAndSearch) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    const std::size_t taskNum = 16;
    for(unsigned int i = 0; i < taskNum; ++i)
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
        if(i % 2 == 0)
        {
            std::string qesBundle;
            std::vector<model::Communication> communications;
            ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));
        }
    }

    {
        const auto bundle = taskGet(kvnr, UrlHelper::escapeUrl("_count=5&status=ready"));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 5);
        EXPECT_EQ(bundle->getTotalSearchMatches(), taskNum / 2);
    }
    {
        const auto bundle = taskGet(
                kvnr, UrlHelper::escapeUrl("_count=10&__offset=5&status=ready"));
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 3);
        EXPECT_EQ(bundle->getTotalSearchMatches(), taskNum / 2);
    }
}

TEST_P(ErpWorkflowTestP, TaskGetAborted) // NOLINT
{
    if (isUnsupportedFlowtype(GetParam()))
    {
        GTEST_SKIP();
    }

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    const std::size_t taskNum = 4;
    for(unsigned int i = 0; i < taskNum; ++i)
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
        std::string qesBundle;
        std::vector<model::Communication> communications;
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));
        std::string secret;
        std::optional<model::Timestamp> lastModifiedDate;
        ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));
        ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, *lastModifiedDate, communications));
        if(i % 2 == 0)
        {
            ASSERT_NO_FATAL_FAILURE(
                taskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), accessCode, secret));
            ASSERT_NO_FATAL_FAILURE(
                taskGetId(*prescriptionId, kvnr, accessCode, std::nullopt,
                           HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
        }
    }

    // Cancelled tasks can also be retrieved since requirement A_19027_03
    auto bundle = taskGet(kvnr).value();
    EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), taskNum);
    EXPECT_EQ(bundle.getTotalSearchMatches(), taskNum);

    A_19027_04.test("Retrieve list of cancelled tasks");
    bundle = taskGet(kvnr, "status=cancelled").value();
    const auto tasks = bundle.getResourcesByType<model::Task>("Task");
    EXPECT_EQ(tasks.size(), taskNum / 2);
    EXPECT_EQ(bundle.getTotalSearchMatches(), taskNum / 2);
    for(const auto& task : tasks)
    {
        EXPECT_EQ(task.status(), model::Task::Status::cancelled);
        EXPECT_TRUE(task.kvnr().has_value());
    }
}

TEST_F(ErpWorkflowTest, AuditEventFilterInsurantKvnr) // NOLINT
{
    std::string kvnr1;
    generateNewRandomKVNR(kvnr1);
    EXPECT_FALSE(kvnr1.empty());

    // Create audit events with kvnr1
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
        std::string qesBundle;
        std::vector<model::Communication> communications;
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr1, accessCode));
        std::string secret;
        std::optional<model::Timestamp> lastModifiedDate;
        ASSERT_NO_FATAL_FAILURE(
            checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr1, accessCode, qesBundle));
        ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr1, secret, *lastModifiedDate, communications));
    }

    std::string kvnr2;
    generateNewRandomKVNR(kvnr2);
    EXPECT_FALSE(kvnr2.empty());

    // Create audit events with kvnr2
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
        std::string qesBundle;
        std::vector<model::Communication> communications;
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr2, accessCode));
        // No access code -> KV-Nr check:
        ASSERT_NO_FATAL_FAILURE(
            checkTaskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr2), kvnr2, {}, {}, communications));
    }

    std::string kvnr3;
    generateNewRandomKVNR(kvnr3);
    EXPECT_FALSE(kvnr3.empty());

    // Create audit events with kvnr3
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
        std::string qesBundle;
        std::vector<model::Communication> communications;
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr3, accessCode));
        std::string secret;
        std::optional<model::Timestamp> lastModifiedDate;
        ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr3, accessCode, qesBundle));
        ASSERT_NO_FATAL_FAILURE(checkTaskReject(*prescriptionId, kvnr3, accessCode, secret));
    }

    ASSERT_NO_FATAL_FAILURE(checkAuditEventsFrom(kvnr1));
    ASSERT_NO_FATAL_FAILURE(checkAuditEventsFrom(kvnr2));
    ASSERT_NO_FATAL_FAILURE(checkAuditEventsFrom(kvnr3));
}

TEST_P(ErpWorkflowTestP, TaskGetRevinclude ) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::string accessCode1;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1, GetParam()));

    std::string qesBundle1;
    std::vector<model::Communication> communications1;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));

    std::optional<model::PrescriptionId> prescriptionId2;
    std::string accessCode2;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2, GetParam()));

    std::string qesBundle2;
    std::vector<model::Communication> communications2;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));


    {
        const auto bundle = taskGetId(*prescriptionId1, kvnr, accessCode1, std::nullopt, HttpStatus::OK, {}, true);
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);

        auto auditEvents = bundle->getResourcesByType<model::AuditEvent>("AuditEvent");
        EXPECT_FALSE(auditEvents.empty());
        for (const auto& item : auditEvents)
        {
            ASSERT_EQ(item.entityDescription(), prescriptionId1->toString());
        }

    }
}

TEST_P(ErpWorkflowTestP, AuditEventSearchSortPaging) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    // Create some audit events
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));
    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));
    // Reject Task
    ASSERT_NO_FATAL_FAILURE(checkTaskReject(*prescriptionId, kvnr, accessCode, secret));
    // May be accepted again
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));
    // Abort task
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId, jwtApotheke(), kvnr, { }, { secret }, communications));

    // Check sorting by subType ascending
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSorting(
        kvnr, "_sort=subtype",
        [](const model::AuditEvent& prev, const model::AuditEvent& cur)
        {
            EXPECT_LE(model::AuditEvent::SubTypeNames.at(prev.subTypeCode()),
                      model::AuditEvent::SubTypeNames.at(cur.subTypeCode()));
        }));

    // Check sorting by subType ascending, date descending
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSorting(
        kvnr, "_sort=subtype,-date",
        [](const model::AuditEvent& prev, const model::AuditEvent& cur)
        {
            const auto prevSubTypeName = model::AuditEvent::SubTypeNames.at(prev.subTypeCode());
            const auto curSubTypeName = model::AuditEvent::SubTypeNames.at(cur.subTypeCode());
            EXPECT_LE(prevSubTypeName, curSubTypeName);
            if(prevSubTypeName == curSubTypeName)
            {
                EXPECT_GE(prev.recorded(), cur.recorded());
            }
        }));

    // Check sorting by subType descending
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSorting(
        kvnr, "_sort=-subtype",
        [](const model::AuditEvent& prev, const model::AuditEvent& cur)
        {
            EXPECT_GE(model::AuditEvent::SubTypeNames.at(prev.subTypeCode()),
                      model::AuditEvent::SubTypeNames.at(cur.subTypeCode()));
        }));

    // Check sorting by date ascending
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSorting(
        kvnr, "_sort=date",
        [](const model::AuditEvent& prev, const model::AuditEvent& cur)
        {
            EXPECT_LE(prev.recorded(), cur.recorded());
        }));

    // Check sorting by date descending
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSorting(
        kvnr, "_sort=-date",
        [](const model::AuditEvent& prev, const model::AuditEvent& cur)
        {
            EXPECT_GE(prev.recorded(), cur.recorded());
        }));

    // Check searching by subType
    ASSERT_NO_FATAL_FAILURE(checkAuditEventSearchSubType(kvnr));

    // Check paging:
    ASSERT_NO_FATAL_FAILURE(checkAuditEventPaging(kvnr, 9, 4));

    // Check paging with additional search:
    ASSERT_NO_FATAL_FAILURE(checkAuditEventPaging(kvnr, 4, 3, "subtype=R"));
}

TEST_F(ErpWorkflowTest, GetMetaData)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::MetaData> metaData;
    ASSERT_NO_FATAL_FAILURE(metaData = metaDataGet(ContentMimeType::fhirJsonUtf8));
    ASSERT_TRUE(metaData);
    if (!runsInCloudEnv())
    {
        const auto releaseDate = model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate().data());
        EXPECT_EQ(metaData->date(), releaseDate);
        EXPECT_EQ(metaData->releaseDate(), releaseDate);
    }
    EXPECT_EQ(metaData->version(), ErpServerInfo::ReleaseVersion());

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData->setVersion(version);
    metaData->setDate(now);
    metaData->setReleaseDate(now);

    const auto* refFile = "test/EndpointHandlerTest/metadata_1.1.1.json";
    if (! model::ResourceVersion::deprecatedProfile(serverGematikProfileVersion()))
    {
        refFile = "test/EndpointHandlerTest/metadata_1.2.json";
    }

    auto expectedMetaData = model::MetaData::fromJsonNoValidation(
        ResourceManager::instance().getStringResource(refFile));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData->serializeToJsonString(), expectedMetaData.serializeToJsonString());
}

TEST_F(ErpWorkflowTest, GetDevice)//NOLINT(readability-function-cognitive-complexity)
{
    std::optional<model::Device> device;

    ASSERT_NO_FATAL_FAILURE(device = deviceGet(ContentMimeType::fhirJsonUtf8));
    ASSERT_TRUE(device);

    EXPECT_EQ(device->id(), "1");
    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device->name(), model::Device::Name);
    EXPECT_TRUE(device->contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device->contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);

    EXPECT_NO_THROW((void)device->serializeToXmlString());
    EXPECT_NO_THROW((void)device->serializeToJsonString());

    ASSERT_NO_FATAL_FAILURE(device = deviceGet(ContentMimeType::fhirXmlUtf8));
    ASSERT_TRUE(device);

    EXPECT_EQ(device->id(), "1");
    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device->name(), model::Device::Name);
    EXPECT_TRUE(device->contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device->contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);

    EXPECT_NO_THROW((void)device->serializeToXmlString());
    EXPECT_NO_THROW((void)device->serializeToJsonString());

}

// Test for https://dth01.ibmgcloud.net/jira/browse/ERP-5387
TEST_P(ErpWorkflowTestP, TaskEmptyOutput)// NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    const std::string kvnr{"X987654326"};
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    // retrieve activated Task
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, kvnr, accessCode));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task,*taskBundle));
    ASSERT_TRUE(task);

    // Empty arrays are not allowed, therefore the Task.output array must not exist at all:
    ASSERT_EQ(rapidjson::Pointer("/output").Get(task->jsonDocument()), nullptr);
}

TEST_F(ErpWorkflowTest, InvalidSearchArguments)
{
    RequestArguments args(HttpMethod::GET, "/Task?authored-on=le2021-04-20T12%3A04%3A56%2B02%3A00&_count=unknown", {});
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(args));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
}

namespace
{
std::string fixBundle(const std::string& bundle, const Uuid& id)
{

    auto result = R"(<?xml version="1.0" encoding="utf-8"?>)""\n" + bundle + "\n";
    // change line end to unix style
    result = regex_replace(result, std::regex{"\r\n"}, "\n");
    // remove extra spaces before tag end (/> and >)
    result = regex_replace(result, std::regex{R"( +(/?>))"}, "$1");
    // remove comments
    result = regex_replace(result, std::regex{R"(<!--.*-->)"}, "$1");
    // fix id
    result = regex_replace(result, std::regex{"8938aff5-720a-414a-b574-114bd8d1e11c"}, id.toString());
    // remove empty lines
    result = regex_replace(result, std::regex{R"((^|\n)\s*\n)"}, "$1");
    // remove redundant namespace declarations
    result = regex_replace(result, std::regex{R"((<[CMPO]\w*) xmlns="http://hl7.org/fhir">)"}, "$1>");
    return result;
}
}

TEST_F(ErpWorkflowTest, EPR_5723_ERP_5750)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());

    const std::string kvnr{"K220645122"};

    auto authoredOn = model::Timestamp::now();
    // prepare QES-Bundle for invocation of POST /task/<id>/$activate
    auto bundleXml = kbvBundleXml({.prescriptionId = prescriptionId.value(), .authoredOn = authoredOn, .kvnr = kvnr});
    auto qesBundle = model::KbvBundle::fromXml(
        bundleXml, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle,
        model::ResourceVersion::supportedBundles());
    std::string qesBundleSigned = toCadesBesSignature(bundleXml, authoredOn);

    std::optional<model::Task> task;
    // invoke /task/<id>/$activate
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(*prescriptionId, accessCode, qesBundleSigned));
    ASSERT_TRUE(task);
    EXPECT_NO_THROW(EXPECT_EQ(task->prescriptionId().toString(), prescriptionId->toString()));
    ASSERT_TRUE(task->kvnr());
    EXPECT_EQ(*task->kvnr(), kvnr);
    EXPECT_EQ(task->status(), model::Task::Status::ready);

   // retrieve activated Task by invoking GET /task/<id>?ac=<accessCode>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, kvnr, accessCode));
    std::vector<model::Bundle> outputBundle;
    ASSERT_NO_THROW(outputBundle = taskBundle->getResourcesByType<model::Bundle>("Bundle"));
    ASSERT_EQ(outputBundle.size(), 1);
    auto xmlDoc{Fhir::instance().converter().jsonToXml(outputBundle[0].jsonDocument())};
    ASSERT_TRUE(xmlDoc);
    std::unique_ptr<xmlChar[],void(*)(void*)> buffer{nullptr, xmlFree};
    int size = 0;
    {
        xmlChar* rawBuffer = nullptr;
        xmlDocDumpFormatMemoryEnc(xmlDoc.get(), &rawBuffer, &size, "utf-8", 1);
        buffer.reset(rawBuffer);
        Expect(size > 0 && rawBuffer != nullptr, "XML document serialization failed.");
    }
    std::string outputBundleXml{reinterpret_cast<const char*>(buffer.get()), gsl::narrow<size_t>(size)};
    // remove signature from outputBundleXml
    // Please note that the regex library on windows uses two constants
    // (_REGEX_MAX_COMPLEXITY_COUNT and _REGEX_MAX_STACK_COUNT) too limit the number
    // of recursive method calls when parsing the pattern for matches. As the pattern
    // "(.|\n)*" leads to many matches between the <signature> tags the stack limit will
    // be exceeded if the test is run on windows machines. Removing the signature by
    // determine the start and end position avoids the regex "stack limit exceeded" exception.
    // The regex is still used afterwards to ensure that also all whitespaces before the
    // signature tags are removed.
    size_t sigStart = outputBundleXml.find("<signature>");
    size_t sigEnd = outputBundleXml.find("</signature>");
    if (sigStart != std::string::npos && sigEnd != std::string::npos)
    {
        sigStart += strlen("<signature>");
        size_t sigLen = sigEnd - sigStart;
        outputBundleXml.replace(sigStart, sigLen, "");
    }
    outputBundleXml = regex_replace(outputBundleXml, std::regex{R"(\n\s*<signature>(.|\n)*</signature>)"}, "");
    EXPECT_EQ(outputBundleXml, fixBundle(bundleXml, outputBundle[0].getId()));
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, kvnr, accessCode));
    // get task again as XML:
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(
            RequestArguments{HttpMethod::GET, "/Task/"s + prescriptionId->toString(), {}}
            .withHeader(Header::Accept, MimeType::fhirXml)
            .withJwt(JwtBuilder::testBuilder().makeJwtVersicherter(kvnr))
    ));
    EXPECT_EQ(response.getHeader().status(), HttpStatus::OK);
    const auto& contentType = response.getHeader().contentType();
    ASSERT_TRUE(contentType.has_value());
    EXPECT_EQ(*contentType,std::string(ContentMimeType::fhirXmlUtf8));
    const auto& body = response.getBody();
    EXPECT_FALSE(body.empty());
    EXPECT_NE(body.find("1\xe2\x80\x93""3mal "), std::string::npos);
}


TEST_F(ErpWorkflowTest, AuditEventWithOptionalClaims) // NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ClientResponse serverResponse;
    std::optional<model::Task> task;

    // Prepare a specialized jwt to test that the claims givenName, lastName and organizationName are optional for LEIs (doctors, for example.)
    static auto constexpr templateResource = "test/jwt/claims_arzt.json";
    auto& resourceManager = ResourceManager::instance();
    const auto& claimTemplate = resourceManager.getJsonResource(templateResource);
    rapidjson::Document claims;
    claims.CopyFrom(claimTemplate, claims.GetAllocator());
    rapidjson::Pointer displayName{"/" + std::string{JWT::displayNameClaim}};
    rapidjson::Pointer organizationName{"/" + std::string{JWT::organizationNameClaim}};
    displayName.Set(claims, rapidjson::Value());
    organizationName.Set(claims, rapidjson::Value());
    JWT jwt = JwtBuilder::testBuilder().makeValidJwt(std::move(claims));

    // Service Call #1: TaskCreate (no audit event generated.)
    using namespace std::string_view_literals;
    using model::Task;
    using model::Timestamp;
    const auto gematikVersion{model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()};
    const auto csFlowtype = (gematikVersion < model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0)
                            ? model::resource::code_system::deprecated::flowType
                            : model::resource::code_system::flowType;
    const std::string create = R"--(<Parameters xmlns="http://hl7.org/fhir">
  <parameter>
    <name value="workflowType"/>
    <valueCoding>
      <system value=")--" + std::string(csFlowtype) + R"--("/>
      <code value="160"/>
    </valueCoding>
  </parameter>
</Parameters>)--";
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(RequestArguments{HttpMethod::POST, "/Task/$create", create, "application/fhir+xml"}.withJwt(jwt)
                                                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
    ASSERT_NO_THROW(task = Task::fromXml(serverResponse.getBody(), *getXmlValidator(),
                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
    // Get results from the service call.
    prescriptionId.emplace(task->prescriptionId());
    accessCode = task->accessCode();

    // Service Call #2: TaskActivate (audit event generated.)
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now())));
    ASSERT_FALSE(qesBundle.empty());
    std::string activateBody = R"(<Parameters xmlns="http://hl7.org/fhir">)""\n"
                               "    <parameter>\n"
                               "      <name value=\"ePrescription\" />\n"
                               "      <resource>\n"
                               "        <Binary>\n"
                               "          <contentType value=\"application/pkcs7-mime\" />\n"
                               "          <data value=\"" + qesBundle + "\" />\n"
                                                                        "        </Binary>\n"
                                                                        "      </resource>\n"
                                                                        "    </parameter>\n"
                                                                        "</Parameters>\n";
    std::string activatePath = "/Task/" + (*prescriptionId).toString() + "/$activate";
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) =
                                send(RequestArguments{mActivateTaskRequestArgs}
                                         .withHttpMethod(HttpMethod::POST)
                                         .withVauPath(activatePath)
                                         .withBody(activateBody)
                                         .withContentType("application/fhir+xml")
                                         .withJwt(jwt)
                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                         .withHeader("X-AccessCode", accessCode)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *getXmlValidator(),
                                                *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
    ASSERT_TRUE(task);
    EXPECT_NO_THROW(EXPECT_EQ(task->prescriptionId().toString(), (*prescriptionId).toString()));
    ASSERT_TRUE(task->kvnr());
    EXPECT_EQ(*task->kvnr(), kvnr);
    EXPECT_EQ(task->status(), model::Task::Status::ready);

    // Check that agentName is "unbekannt".
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", "date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=date"));
    ASSERT_TRUE(auditEventBundle);
    ASSERT_EQ(auditEventBundle->getResourceCount(), 1);
    const auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    const auto &auditEvent = auditEvents[0];
    EXPECT_EQ("unbekannt", auditEvent.agentName());
}

// Test cases of
// 1. POST /Task/$close where the prescription id of the received medication dispense is invalid (see also ticket ERP-5805).
// 2. POST /Task/$close with invalid field "whenHandedOver" to verify the diagnostics data contained in the OperationOutcome
//    resource of the error response.
TEST_P(ErpWorkflowTestP, TaskClose_MedicationDispense_invalidPrescriptionIdAndWhenHandedOver) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task);
    std::string accessCode(task->accessCode());

    const std::string kvnr{"X007654320"};
    const auto [qesBundle, _] = makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, qesBundle));
    ASSERT_TRUE(task);

    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(task->prescriptionId(), accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), 2);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    const auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());
    ASSERT_TRUE(tasks[0].owner().has_value());
    ASSERT_EQ(tasks[0].owner(), jwtApotheke().stringForClaim(JWT::idNumberClaim));

    // Invalid format of prescriptionId for Medication dispense, will be rejected by schema check:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidPrescriptionId(
        task->prescriptionId(), "INVALID", std::string(secret.value()), kvnr));
    // Valid format of prescriptionId for Medication dispense but invalid content:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidPrescriptionId(
        task->prescriptionId(), "160.000.000.000.000.00", std::string(secret.value()), kvnr));
    // Valid format and content of prescriptionId for Medication dispense but does not correspond to Task prescriptionId:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidPrescriptionId(
        task->prescriptionId(), "160.005.363.425.241.41", std::string(secret.value()), kvnr));

    // Invalid values for field "whenHandedOver", check correct values in OperationOutcome:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidWhenHandedOver(
        task->prescriptionId(), std::string(secret.value()), kvnr, "invalid-Date"));
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidWhenHandedOver(
        task->prescriptionId(), std::string(secret.value()), kvnr, "2021-09-21T07:13:00+00:00.1234"));
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispense_invalidWhenHandedOver(
        task->prescriptionId(), std::string(secret.value()), kvnr, "2021-14-12"));
}

TEST_P(ErpWorkflowTestP, TaskAbort_NewlyCreated) // NOLINT
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task);
    const std::string accessCode(task->accessCode());
    ASSERT_NO_FATAL_FAILURE(
        taskAbort(task->prescriptionId(), JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {},
                  HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
}

TEST_P(ErpWorkflowTestP, TaskCancelled) // NOLINT
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task);
    const std::string accessCode(task->accessCode());
    const auto prescriptionId = task->prescriptionId();
    const std::string kvnr{"X101010104"};

    const auto [qesBundle, _] = makeQESBundle(kvnr, prescriptionId, model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(prescriptionId, accessCode, qesBundle));
    ASSERT_TRUE(task);

    // Abort task:
    ASSERT_NO_FATAL_FAILURE(taskAbort(prescriptionId, JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {}));

    // The following calls all should return with error HttpStatus::Gone:
    ASSERT_NO_FATAL_FAILURE(taskGetId(prescriptionId, kvnr, accessCode, std::nullopt,
                                      HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_NO_FATAL_FAILURE(taskAbort(prescriptionId, JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {},
                                      HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(prescriptionId, accessCode, qesBundle,
                                         HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_NO_FATAL_FAILURE(taskAccept(prescriptionId, accessCode,
                                       HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_NO_FATAL_FAILURE(taskClose(prescriptionId, {}, kvnr,
                                      HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_NO_FATAL_FAILURE(taskReject(prescriptionId, {},
                                       HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
}

TEST_F(ErpWorkflowTest, RejectTaskInvalidId) // NOLINT
{
    // This is already failing in the VauRequestHandler (tests therefore also the other Task endpoints with id) :
    ASSERT_NO_FATAL_FAILURE(taskReject("thi$-is_an-invalid-ta$k-id_#§&?ß", {},
                                       HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

namespace
{
    void checkJsonString(
        const rapidjson::Document& document,
        const rapidjson::Pointer& jsonPointer,
        const std::optional<std::string>& expected)
    {
        using mrb = model::ResourceBase;
        ASSERT_NE(jsonPointer.Get(document), nullptr) << "Element " << mrb::pointerToString(jsonPointer) << " is missing";
        ASSERT_TRUE(jsonPointer.Get(document)->IsString()) << "Element " << mrb::pointerToString(jsonPointer) << " is no string";
        if(expected.has_value())
        {
            EXPECT_EQ(std::string(jsonPointer.Get(document)->GetString()), expected.value());
        }
    }
    void checkJsonInt(
        const rapidjson::Document& document,
        const rapidjson::Pointer& jsonPointer,
        const std::optional<int>& expected)
    {
        using mrb = model::ResourceBase;
        ASSERT_NE(jsonPointer.Get(document), nullptr) << "Element " << mrb::pointerToString(jsonPointer) << " is missing";
        ASSERT_TRUE(jsonPointer.Get(document)->IsInt()) << "Element " << mrb::pointerToString(jsonPointer) << " is no integer";
        if(expected.has_value())
        {
            EXPECT_EQ(jsonPointer.Get(document)->GetInt(), expected.value());
        }
    }
} // anonymous namespace

TEST_F(ErpWorkflowTest, OuterErrorResponse) // NOLINT
{
    ClientResponse outerResponse;
    ClientResponse innerResponse;
    const JWT jwt = defaultJwt();

    const rapidjson::Pointer xRequestIdPointer ("/x-request-id");
    const rapidjson::Pointer statusPointer ("/status");
    const rapidjson::Pointer errorPointer ("/error");
    const rapidjson::Pointer messagePointer ("/message");

    rapidjson::Document outerErrorResponseDocument;

    ASSERT_NO_FATAL_FAILURE(
        std::tie(outerResponse, innerResponse) =
            send(RequestArguments{HttpMethod::GET, "/Task/", {}}.withJwt(jwt),
                 [](std::string& request){ request[2]++; }));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::BadRequest);
    ASSERT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
    EXPECT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), "application/json");

    outerErrorResponseDocument.Parse(outerResponse.getBody());

    checkJsonString(outerErrorResponseDocument, xRequestIdPointer, {});
    if (!runsInCloudEnv())
    {
        const auto* requestIdPtr = xRequestIdPointer.Get(outerErrorResponseDocument);
        ASSERT_NE(requestIdPtr, nullptr);
        const std::string xRequestId = requestIdPtr->GetString();
        // the proxy modifies this field to something like:
        // "[b75bd220-7db0-4df2-8f17-0e0b9b36fca6, 79f758df-2eb8-4b53-b766-4950e608bdef]"
        EXPECT_TRUE(Uuid(xRequestId).isValidIheUuid()) << xRequestId;
    }
    checkJsonInt(outerErrorResponseDocument, statusPointer, 400);
    checkJsonString(outerErrorResponseDocument, errorPointer,
                    "vau decryption failed: ErpException Unable to create public key from message data");
    checkJsonString(outerErrorResponseDocument, messagePointer,
                    "could not create public key from x,y components error:0800006B:elliptic curve routines:"
                    ":point is not on curve");

    ASSERT_NO_FATAL_FAILURE(
        std::tie(outerResponse, innerResponse) =
            send(RequestArguments{HttpMethod::GET, "/Task/", {}}.withJwt(jwt),
                [](std::string& request){ request[request.size()/2]++; }));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::BadRequest);
    EXPECT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
    EXPECT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), "application/json");

    outerErrorResponseDocument.Parse(outerResponse.getBody());

    checkJsonString(outerErrorResponseDocument, xRequestIdPointer, {});
    if (!runsInCloudEnv())
    {
        const auto* requestIdPtr = xRequestIdPointer.Get(outerErrorResponseDocument);
        ASSERT_NE(requestIdPtr, nullptr);
        EXPECT_TRUE(Uuid(requestIdPtr->GetString()).isValidIheUuid());
    }
    checkJsonInt(outerErrorResponseDocument, statusPointer, 400);
    checkJsonString(outerErrorResponseDocument, errorPointer,
                    "vau decryption failed: AesGcmException can't finalize AES-GCM decryption");
    EXPECT_EQ(messagePointer.Get(outerErrorResponseDocument), nullptr); // field "message" does not exist;
}

TEST_P(ErpWorkflowTestP, ErrorResponseNoInnerRequest) // NOLINT
{
    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    const std::string kvnr{"X987654326"};
    auto medicationDipenseRenderVersion =
        model::ResourceVersion::fhirProfileBundleFromSchemaVersion(serverGematikProfileVersion());
    const auto closeBody = medicationDispense(kvnr, prescriptionId->toString(), model::Timestamp::now().toGermanDate(),
                                              medicationDipenseRenderVersion);
    const std::string closePath = "/Task/" + prescriptionId->toString() + "/$close?secret=XXXXX" ;
    const JWT jwt{ jwtApotheke() };
    RequestArguments args{HttpMethod::POST, closePath, closeBody, "application/fhir+xml", false};
    args.jwt = jwt;
    args.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    args.overrideExpectedInnerOperation = "UNKNOWN";
    args.overrideExpectedInnerRole = "XXX";
    args.overrideExpectedInnerClientId = "XXX";
    args.overrideExpectedLeips = "XXX";
    args.overrideExpectedPrescriptionId = "XXX";
    ClientResponse innerResponse;

    // Send request with missing ContentLength header
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, innerResponse) =
            send(args, {}, [](Header& header){ header.removeHeaderField(Header::ContentLength); }));
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentType));
    ASSERT_EQ(innerResponse.getHeader().header(Header::ContentType).value(), "application/fhir+xml;charset=utf-8");
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), false /*Json*/), model::OperationOutcome::Issue::Type::invalid,
        "HTTP parser finished before end of message, Content-Length field is missing or the value is too low.");

    // Send request with ContentLength header containing too low value
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, innerResponse) =
            send(args, {}, [](Header& header){ header.addHeaderField(Header::ContentLength, "200"); }));
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentType));
    ASSERT_EQ(innerResponse.getHeader().header(Header::ContentType).value(), "application/fhir+xml;charset=utf-8");
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), false /*Json*/), model::OperationOutcome::Issue::Type::invalid,
        "HTTP parser finished before end of message, Content-Length field is missing or the value is too low.");

    // Send request with ContentLength header containing too high value
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, innerResponse) =
            send(args, {}, [](Header& header){ header.addHeaderField(Header::ContentLength, "999999"); }));
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentType));
    ASSERT_EQ(innerResponse.getHeader().header(Header::ContentType).value(), "application/fhir+xml;charset=utf-8");
    checkOperationOutcome(operationOutcomeFromResponse(innerResponse.getBody(), false /*Json*/),
                          model::OperationOutcome::Issue::Type::invalid,
                          "HTTP parser did not finish correctly, Content-Length field is too large.");
}

TEST_F(ErpWorkflowTest, InnerRequestFlowtype) // NOLINT
{
    if (runsInCloudEnv())
    {
        GTEST_SKIP_("skipped in cloud environment");
    }
    std::string accessCode;
    ClientResponse outerResponse;

    const model::PrescriptionId prescriptionId_apothekenpflichigeArzneimittel =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 999999);
    const std::string activePath = "/Task/" + prescriptionId_apothekenpflichigeArzneimittel.toString() + "/$activate";
    RequestArguments args{HttpMethod::POST, activePath, {}};
    args.jwt = JwtBuilder::testBuilder().makeJwtArzt();

    // Send request with PrescriptionType =  apothekenpflichigeArzneimittel
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, std::ignore) = send(args));
    EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestFlowtype).value(),
        std::to_string(static_cast<std::underlying_type_t<model::PrescriptionType>>(model::PrescriptionType::apothekenpflichigeArzneimittel)));


    // Send request with PrescriptionType =  direkteZuweisung
    const model::PrescriptionId prescriptionId_direkteZuweisung =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 999999);
    args.vauPath = "/Task/" + prescriptionId_direkteZuweisung.toString() + "/$activate";

    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, std::ignore) = send(args));
    EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestFlowtype).value(),
        std::to_string(static_cast<std::underlying_type_t<model::PrescriptionType>>(model::PrescriptionType::direkteZuweisung)));

    // Send request with operation $reject
    args.vauPath = "/Task/" + prescriptionId_direkteZuweisung.toString() + "/$reject";
    args.jwt = JwtBuilder::testBuilder().makeJwtApotheke();
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, std::ignore) = send(args));
    EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestFlowtype).value(),
              std::to_string(static_cast<std::underlying_type_t<model::PrescriptionType>>(model::PrescriptionType::direkteZuweisung)));

    // Send request with HttpMethod::GET
    args.vauPath = "/MedicationDispense/" + prescriptionId_direkteZuweisung.toString();
    args.method = HttpMethod::GET;
    args.jwt = defaultJwt();
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, std::ignore) = send(args));
    EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestFlowtype).value_or("XXX"),
              std::to_string(static_cast<std::underlying_type_t<model::PrescriptionType>>(
                  model::PrescriptionType::direkteZuweisung)));
}

TEST_P(ErpWorkflowTestP, OperationOutcomeIncodeValidation)// NOLINT
{
    if (!model::ResourceVersion::supportedBundles()
            .contains(model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01))
    {
        GTEST_SKIP_("Incode-validation only applicable for 2022-01-01 profiles.");
    }
    if (model::IsPkv(GetParam()))
    {
        GTEST_SKIP_("PKV not testable with old profiles");
    }
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    auto bundle =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_invalid_erp_8431.xml");
    bundle = String::replaceAll(bundle, "REPLACE_ME_taskId", prescriptionId->toString());
    const auto& qes = toCadesBesSignature(bundle, model::Timestamp::fromXsDate("2021-06-08", model::Timestamp::UTCTimezone));
    ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(*prescriptionId, accessCode, qes, HttpStatus::BadRequest,
                                         model::OperationOutcome::Issue::Type::invalid, "mandatory identifier.value not set"));
}

TEST_P(ErpWorkflowTestP, SearchCommunicationsByReceivedTimeRange) // NOLINT
{
    if (serverUsesOldProfile() && model::IsPkv(GetParam()))
    {
        GTEST_SKIP_("PKV not testable with old profiles");
    }
    using namespace std::chrono_literals;

    // invoke POST /task/$create
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
    ASSERT_TRUE(task);

    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));

    std::optional<model::PrescriptionId> prescriptionId = task->prescriptionId();
    ASSERT_TRUE(prescriptionId);
    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now())));

    // invoke /task/<id>/$activate
    ASSERT_NO_FATAL_FAILURE(task = taskActivateWithOutcomeValidation(*prescriptionId, std::string(task->accessCode()), qesBundle));
    ASSERT_TRUE(task);

    const auto telematicId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematicId.has_value());

    std::optional<model::Communication> communicationResponseA;
    ASSERT_NO_FATAL_FAILURE(communicationResponseA = communicationPost(
        model::Communication::MessageType::InfoReq, task.value(), ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(), "Ist das Medikament A bei Ihnen vorrätig?"));

    std::this_thread::sleep_for(1s);

    std::optional<model::Communication> communicationResponseB;
    ASSERT_NO_FATAL_FAILURE(communicationResponseB = communicationPost(
        model::Communication::MessageType::InfoReq, task.value(), ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(), "Ist das Medikament B bei Ihnen vorrätig?"));

    std::this_thread::sleep_for(2s);

    ASSERT_TRUE(communicationResponseA->timeSent().has_value());
    auto args = "sent=ge" + communicationResponseA->timeSent()
                                ->toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone)
                                .substr(0, 19);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtApotheke(), args)); // sets "received" timstamp
    EXPECT_EQ(communicationsBundle->getResourceCount(), 2);

    auto communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    ASSERT_TRUE(communications[0].timeReceived().has_value());
    const auto start = communications[0].timeReceived().value() + (-1s);

    std::optional<model::Communication> communicationResponseC;
    ASSERT_NO_FATAL_FAILURE(communicationResponseC = communicationPost(
        model::Communication::MessageType::InfoReq, task.value(), ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(), "Ist das Medikament C bei Ihnen vorrätig?"));
    std::optional<model::Communication> communicationResponseD;
    ASSERT_NO_FATAL_FAILURE(communicationResponseD = communicationPost(
        model::Communication::MessageType::InfoReq, task.value(), ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(), "Ist das Medikament D bei Ihnen vorrätig?"));

    std::this_thread::sleep_for(1s);

    ASSERT_TRUE(communicationResponseC->timeSent().has_value());
    args = "sent=ge" + communicationResponseC->timeSent()
                           ->toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone)
                           .substr(0, 19);
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtApotheke(), args)); // sets "received" timstamp

    communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    ASSERT_TRUE(communications[0].timeReceived().has_value());
    const auto end = communications[0].timeReceived().value();

    EXPECT_EQ(communicationsBundle->getResourceCount(), 2);

    const auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    args = "received=gt" + start.toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19) +
           "&received=lt" + end.toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone).substr(0, 19) +
           "&_sort=sent";
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtApotheke(), args));
    ASSERT_EQ(communicationsBundle->getResourceCount(), 2);
    communications =  communicationsBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    EXPECT_EQ(communications[0].id(), communicationResponseA->id());
    EXPECT_EQ(communications[1].id(), communicationResponseB->id());

}

TEST_F(ErpWorkflowTest, CommunicationJsonValidationError)
{
    std::string communication =
        R"___({"resourceType":"Communication","meta":{"lastUpdated":null,"profile":["https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq"]},"basedOn":[{"reference":"Task/160.000.226.093.129.57/$accept?ac=5edb4d3d90e21fbba2b72855e851e8e5dc7ca1ef470a727103d069627687c8b4","type":null,"identifier":null,"display":null}],"status":"unknown","recipient":[{"reference":null,"type":null,"identifier":{"use":null,"type":null,"system":"https://gematik.de/fhir/sid/telematik-id","value":"3-10.2.0110201000.579","period":null},"display":null}],"identifier":{"use":null,"type":null,"system":"https://gematik.de/fhir/NamingSystem/OrderID","value":"6e59e060-0131-4020-ad05-1b4196a75f5e","period":null},"contained":[],"payload":[{"extension":null,"contentString":"{\"supplyOptionsType\":\"onPremise\",\"hint\":\"\"}"}]})___";
    RequestArguments reqArgs(HttpMethod::POST, "/Communication", communication, "application/json");
    auto [outerResonse, innerResponse] = send(reqArgs);
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), true), model::OperationOutcome::Issue::Type::invalid,
        "validation of JSON document failed. FHIR JSON schema can be retrieved here: "
        "https://hl7.org/fhir/R4/fhir.schema.json.zip"/*,
        "{\"expected\":[\"string\"],\"actual\":\"null\",\"errorCode\":20,\"instanceRef\":\"#/meta/"
        "lastUpdated\",\"schemaRef\":\"/home/jens/erp/erp-processing-context/cmake-build-debug-wsl-gcc-12/bin/"
        "resources/schema/hl7.fhir.r4.core/4.0.1/json/fhir.json#/definitions/instant\"}"*/);
}

TEST_F(ErpWorkflowTest, CommunicationPayloadJsonValidationError)
{
    if (serverUsesOldProfile())
    {
        GTEST_SKIP();
    }
    auto task = taskCreate();
    auto kvnr = generateNewRandomKVNR();
    taskActivate(task->prescriptionId(), task->accessCode(),
                 std::get<0>(makeQESBundle(kvnr.id(), task->prescriptionId(), model::Timestamp::now())));
    auto profileVersion = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    CommunicationJsonStringBuilder builder(model::Communication::MessageType::DispReq, profileVersion);
    builder.setPayload(
        R"___({ "version": 1, "supplyOptionsType": "invalid", "name": "Dr. Maximilian von Muster", "address": [ "wohnhaft bei Emilia Fischer", "Bundesallee 312", "123. OG", "12345 Berlin" ], "hint": "Bitte im Morsecode klingeln: -.-.", "phone": "004916094858168" })___");
    builder.setPrescriptionId(task->prescriptionId().toString());
    builder.setRecipient(ActorRole::Pharmacists, "3-10.2.0110201000.579");
    builder.setAccessCode(std::string{task->accessCode()});
    auto communication = builder.createJsonString();
    auto reqArgs = RequestArguments(HttpMethod::POST, "/Communication", communication, "application/json")
                       .withHeader(Header::XAccessCode, std::string{task->accessCode()});
    reqArgs.overrideExpectedPrescriptionId = task->prescriptionId().toString();
    auto [outerResonse, innerResponse] = send(reqArgs);
    checkOperationOutcome(
        operationOutcomeFromResponse(innerResponse.getBody(), true), model::OperationOutcome::Issue::Type::invalid,
        "Invalid payload: does not conform to expected JSON schema: validation of JSON document failed"/*,
        "{\"enum\":{\"errorCode\":19,\"instanceRef\":\"#/supplyOptionsType\",\"schemaRef\":\"/home/jens/erp/"
        "erp-processing-context/cmake-build-debug-wsl-gcc-12/bin/resources/schema/shared/json/"
        "CommunicationDispReqPayload.json#/properties/supplyOptionsType\"}}"*/);
}

INSTANTIATE_TEST_SUITE_P(ErpWorkflowTestPInst, ErpWorkflowTestP,
                         testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                         model::PrescriptionType::direkteZuweisung,
                                         model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                         model::PrescriptionType::direkteZuweisungPkv));
