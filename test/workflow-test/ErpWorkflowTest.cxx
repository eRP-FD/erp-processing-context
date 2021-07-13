#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "test/util/StaticData.hxx"
#include "tools/ResourceManager.hxx"

#include <thread>

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

TEST_F(ErpWorkflowTest, TaskLifecycleNormal)// NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    const std::string kvnr{"X987654321"};
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    // invoke /task/<id>/$accept
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle));

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
    }

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        *prescriptionId, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr, kvnr }, { 0, 2, 3, 4, 5 }, { 7 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::read});
}

TEST_F(ErpWorkflowTest, TaskLifecycleAbortByInsurantProxy) // NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    const std::string kvnr{"X123456789"};
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::string kvnrRepresentative;
    for (const auto& communication : communications)
    {
        if (communication.messageType() == model::Communication::MessageType::Representative && communication.sender() != kvnr)
        {
            kvnrRepresentative = std::string(communication.recipient().value());
            break;
        }
    }
    ASSERT_FALSE(kvnrRepresentative.empty());
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnrRepresentative), kvnr, { accessCode }, { }, communications));

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        *prescriptionId, kvnr, "en", startTime,
        { telematicIdDoctor, kvnr, kvnrRepresentative }, { 0 }, { },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::del});
}

TEST_F(ErpWorkflowTest, TaskLifecycleAbortByInsurant) // NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    // No access code -> KV-Nr check:
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), kvnr, { }, { }, communications));

    // Check for correct return code in this case, for bugticket ERP-4972
    taskAccept(*prescriptionId,accessCode, HttpStatus::Gone);

    // check no task is returned for GetAllTasks
    auto tasks = taskGet(kvnr);
    ASSERT_TRUE(tasks);
    ASSERT_EQ(tasks->getResourceCount(), 0);

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        *prescriptionId, kvnr, "de", startTime,
        { kvnr, telematicIdDoctor, kvnr, kvnr, kvnr }, { 1 }, { 0, 4 },
        { model::AuditEvent::SubType::read, model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::del, model::AuditEvent::SubType::read });
}

TEST_F(ErpWorkflowTest, TaskLifecycleAbortByPharmacy) // NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    const std::string kvnr{"X123456787"};
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
        *prescriptionId, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy }, { 0, 2, 3, 4 }, { },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::del });
}

TEST_F(ErpWorkflowTest, TaskLifecycleReject) // NOLINT
{
    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    const std::string kvnr{"X123456786"};
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
        *prescriptionId, kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr,
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, kvnr}, { 0, 2, 3, 4, 6, 7, 8, 9 }, { },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::read });
}

TEST_F(ErpWorkflowTest, TaskSearchStatus) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3));

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
        ASSERT_NO_FATAL_FAILURE(taskGet(kvnr, "modified=eq2021-06-18&status=eqREADY", HttpStatus::BadRequest));
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
    ASSERT_NO_FATAL_FAILURE(checkTaskAbort(*prescriptionId3, jwtVersicherter(), kvnr, accessCode3, {}, communications3));

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
         // cancelled tasks cannot be retrieved by GET
        const auto bundle = taskGet(kvnr, "status=cancelled").value();
        EXPECT_EQ(bundle.getResourceCount(), 0);
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
    ClientResponse outerResponse;
    ClientResponse innerResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, innerResponse) = send(args));
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);

}

TEST_F(ErpWorkflowTest, TaskSearchLastModified) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3));

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

TEST_F(ErpWorkflowTest, TaskSearchAuthoredOn ) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));
    std::this_thread::sleep_for(1s);
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3));

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
    const auto dateTimeWithoutTz1 = dateTime1.substr(0, 19); // will be interpreted as UTC;
    const auto dateTimeWithoutTz2 = dateTime2.substr(0, 19);

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

TEST_F(ErpWorkflowTest, TaskGetPaging ) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3));

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

TEST_F(ErpWorkflowTest, TaskGetSorting ) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::optional<model::PrescriptionId> prescriptionId2;
    std::optional<model::PrescriptionId> prescriptionId3;
    std::string accessCode1;
    std::string accessCode2;
    std::string accessCode3;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId3, accessCode3));

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

TEST_F(ErpWorkflowTest, TaskGetPagingAndSearch) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    const std::size_t taskNum = 16;
    for(unsigned int i = 0; i < taskNum; ++i)
    {
        std::optional<model::PrescriptionId> prescriptionId;
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
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

TEST_F(ErpWorkflowTest, TaskGetRevinclude ) // NOLINT
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    std::optional<model::PrescriptionId> prescriptionId1;
    std::string accessCode1;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId1, accessCode1));

    std::string qesBundle1;
    std::vector<model::Communication> communications1;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle1, communications1, *prescriptionId1, kvnr, accessCode1));

    std::optional<model::PrescriptionId> prescriptionId2;
    std::string accessCode2;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId2, accessCode2));

    std::string qesBundle2;
    std::vector<model::Communication> communications2;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle2, communications2, *prescriptionId2, kvnr, accessCode2));


    {
        const auto bundle = taskGetId(*prescriptionId1, kvnr, accessCode1, HttpStatus::OK, true);
        EXPECT_EQ(bundle->getResourcesByType<model::Task>("Task").size(), 1);

        auto auditEvents = bundle->getResourcesByType<model::AuditEvent>("AuditEvent");
        EXPECT_FALSE(auditEvents.empty());
        for (const auto& item : auditEvents)
        {
            ASSERT_EQ(item.entityDescription(), prescriptionId1->toString());
        }

    }
}

TEST_F(ErpWorkflowTest, AuditEventSearchSortPaging) // NOLINT
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

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
    ASSERT_NO_FATAL_FAILURE(checkAuditEventPaging(kvnr, 10, 4));

    // Check paging with additional search:
    ASSERT_NO_FATAL_FAILURE(checkAuditEventPaging(kvnr, 5, 3, "subtype=R"));
}

TEST_F(ErpWorkflowTest, GetMetaData)
{
    std::optional<model::MetaData> metaData;
    ASSERT_NO_FATAL_FAILURE(metaData = metaDataGet(ContentMimeType::fhirJsonUtf8));
    ASSERT_TRUE(metaData);
    const auto releaseDate = model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate);
    EXPECT_EQ(metaData->date(), releaseDate);
    EXPECT_EQ(metaData->releaseDate(), releaseDate);
    EXPECT_EQ(metaData->version(), ErpServerInfo::ReleaseVersion);

    const auto now = model::Timestamp::now();
    const auto* version = "0.3.1";
    metaData->setVersion(version);
    metaData->setDate(now);
    metaData->setReleaseDate(now);

    auto expectedMetaData = model::MetaData::fromJson(FileHelper::readFileAsString(dataPath + "/metadata.json"));
    expectedMetaData.setVersion(version);
    expectedMetaData.setDate(now);
    expectedMetaData.setReleaseDate(now);

    ASSERT_EQ(metaData->serializeToJsonString(), expectedMetaData.serializeToJsonString());
}

TEST_F(ErpWorkflowTest, GetDevice)
{
    std::optional<model::Device> device;

    ASSERT_NO_FATAL_FAILURE(device = deviceGet(ContentMimeType::fhirJsonUtf8));
    ASSERT_TRUE(device);

    EXPECT_EQ(device->id(), "1");
    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->name(), model::Device::Name);
    EXPECT_TRUE(device->contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device->contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);

    EXPECT_NO_THROW((void)device->serializeToXmlString());
    EXPECT_NO_THROW((void)device->serializeToJsonString());

    ASSERT_NO_FATAL_FAILURE(device = deviceGet(ContentMimeType::fhirXmlUtf8));
    ASSERT_TRUE(device);

    EXPECT_EQ(device->id(), "1");
    EXPECT_EQ(device->status(), model::Device::Status::active);
    EXPECT_EQ(device->serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device->name(), model::Device::Name);
    EXPECT_TRUE(device->contact(model::Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device->contact(model::Device::CommunicationSystem::email).value(), model::Device::Email);

    EXPECT_NO_THROW((void)device->serializeToXmlString());
    EXPECT_NO_THROW((void)device->serializeToJsonString());

}

// Test for https://dth01.ibmgcloud.net/jira/browse/ERP-5387
TEST_F(ErpWorkflowTest, TaskEmptyOutput)// NOLINT
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    const std::string kvnr{"X987654321"};
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
    result = regex_replace(result, std::regex{"9581ce65-b118-4751-9073-19c091b341e0"}, id.toString());
    // remove empty lines
    result = regex_replace(result, std::regex{R"((^|\n)\s*\n)"}, "$1");
    // remove redundant namespace declarations
    result = regex_replace(result, std::regex{R"((<[CMPO]\w*) xmlns="http://hl7.org/fhir">)"}, "$1>");
    return result;
}
}

TEST_F(ErpWorkflowTest, EPR_5723_ERP_5750)
{
    using namespace std::string_literals;
    auto& resourceManager = ResourceManager::instance();
    auto qesXMLString = resourceManager.getStringResource("test/issues/ERP-5723_ERP-5750/InputKbvBundle.xml");
    auto qesBundle =
            model::Bundle::fromXml(qesXMLString, *StaticData::getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle);
    std::string qesBundleSigned = toCadesBesSignature(qesXMLString);
    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());

    const std::string kvnr{"K220645129"};
       // repare QES-Bundle for invokation of POST /task/<id>/$activate

    std::optional<model::Task> task;
    // invoke /task/<id>/$activate
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(*prescriptionId, accessCode, qesBundleSigned));
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
    EXPECT_EQ(outputBundleXml, fixBundle(qesXMLString, outputBundle[0].getId()));
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
    EXPECT_NE(body.find("1\xe2\x80\x93""3mal/Tag"), std::string::npos);
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
    rapidjson::Pointer givenName{"/" + std::string{JWT::givenNameClaim}};
    rapidjson::Pointer lastName{"/" + std::string{JWT::familyNameClaim}};
    rapidjson::Pointer organizationName{"/" + std::string{JWT::organizationNameClaim}};
    givenName.Set(claims, rapidjson::Value());
    lastName.Set(claims, rapidjson::Value());
    organizationName.Set(claims, rapidjson::Value());
    JWT jwt = JwtBuilder::testBuilder().makeValidJwt(std::move(claims));

    // Service Call #1: TaskCreate (no audit event generated.)
    using namespace std::string_view_literals;
    using model::Task;
    using model::Timestamp;
    static const char* create =
        "<Parameters xmlns=\"http://hl7.org/fhir\">\n"
        "  <parameter>\n"
        "    <name value=\"workflowType\"/>\n"
        "    <valueCoding>\n"
        "      <system value=\"https://gematik.de/fhir/CodeSystem/Flowtype\"/>\n"
        "      <code value=\"160\"/>\n"
        "    </valueCoding>\n"
        "  </parameter>\n"
        "</Parameters>\n";
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(RequestArguments{HttpMethod::POST, "/Task/$create", create, "application/fhir+xml"}.withJwt(jwt)
                                                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
    ASSERT_NO_THROW(task = Task::fromXml(serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxTask));
    // Get results from the service call.
    prescriptionId.emplace(task->prescriptionId());
    accessCode = task->accessCode();

    // Service Call #2: TaskActivate (audit event generated.)
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_THROW(qesBundle = makeQESBundle(kvnr, *prescriptionId, model::Timestamp::now()));
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
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(RequestArguments{HttpMethod::POST, activatePath, activateBody, "application/fhir+xml"}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withHeader("X-AccessCode", accessCode)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxTask));
    ASSERT_TRUE(task);
    EXPECT_NO_THROW(EXPECT_EQ(task->prescriptionId().toString(), (*prescriptionId).toString()));
    ASSERT_TRUE(task->kvnr());
    EXPECT_EQ(*task->kvnr(), kvnr);
    EXPECT_EQ(task->status(), model::Task::Status::ready);

    // Check that agentName is "<null>".
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", "date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=date"));
    ASSERT_TRUE(auditEventBundle);
    ASSERT_EQ(auditEventBundle->getResourceCount(), 2); // the first auditevent comes from generateNewRandomKVNR
    const auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    const auto &auditEvent = auditEvents[1];
    EXPECT_EQ("<null>", auditEvent.agentName());
}

// Test cases of POST /Task/$close where the prescription id of the received medication dispense
// is invalid (see also ticket ERP-5805):
TEST_F(ErpWorkflowTest, TaskClose_MedicationDispenseInvalidPrescriptionId) // NOLINT
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task);
    std::string accessCode(task->accessCode());

    const std::string kvnr{"X007654321"};
    std::string qesBundle = makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(task->prescriptionId(), accessCode, qesBundle));
    ASSERT_TRUE(task);

    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(task->prescriptionId(), accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), 2);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    const auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());

    // Invalid format of prescriptionId for Medication dispense, will be rejected by schema check:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispenseInvalidPrescriptionId(
        task->prescriptionId(), "INVALID", std::string(secret.value()), kvnr));
    // Valid format of prescriptionId for Medication dispense but invalid content:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispenseInvalidPrescriptionId(
        task->prescriptionId(), "160.000.000.000.000.00", std::string(secret.value()), kvnr));
    // Valid format and content of prescriptionId for Medication dispense but does not correspond to Task prescriptionId:
    ASSERT_NO_FATAL_FAILURE(taskClose_MedicationDispenseInvalidPrescriptionId(
        task->prescriptionId(), "160.005.363.425.241.41", std::string(secret.value()), kvnr));
}

TEST_F(ErpWorkflowTest, TaskAbort_NewlyCreated) // NOLINT
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task);
    const std::string accessCode(task->accessCode());
    ASSERT_NO_FATAL_FAILURE(
        taskAbort(task->prescriptionId(), JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {}, HttpStatus::Forbidden));
}

TEST_F(ErpWorkflowTest, TaskCancelled) // NOLINT
{
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task);
    const std::string accessCode(task->accessCode());
    const auto prescriptionId = task->prescriptionId();
    const std::string kvnr{"X101010101"};

    const std::string qesBundle = makeQESBundle(kvnr, prescriptionId, model::Timestamp::now());
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(prescriptionId, accessCode, qesBundle));
    ASSERT_TRUE(task);

    // Abort task:
    ASSERT_NO_FATAL_FAILURE(taskAbort(prescriptionId, JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {}));

    // The following calls all should return with error HttpStatus::Gone:
    ASSERT_NO_FATAL_FAILURE(taskGetId(prescriptionId, kvnr, accessCode, HttpStatus::Gone));
    ASSERT_NO_FATAL_FAILURE(taskAbort(prescriptionId, JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {}, HttpStatus::Gone));
    ASSERT_NO_FATAL_FAILURE(taskActivate(prescriptionId, accessCode, qesBundle, HttpStatus::Gone));
    ASSERT_NO_FATAL_FAILURE(taskAccept(prescriptionId, accessCode, HttpStatus::Gone));
    ASSERT_NO_FATAL_FAILURE(taskClose(prescriptionId, {}, kvnr, HttpStatus::Gone));
    ASSERT_NO_FATAL_FAILURE(taskReject(prescriptionId, {}, HttpStatus::Gone));
}

TEST_F(ErpWorkflowTest, RejectTaskInvalidId) // NOLINT
{
    // This is already failing in the VauRequestHandler (tests therefore also the other Task endpoints with id) :
    ASSERT_NO_FATAL_FAILURE(taskReject("thi$-is_an-invalid-ta$k-id_#§&?ß", {}, HttpStatus::BadRequest));
}
