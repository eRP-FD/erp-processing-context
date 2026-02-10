/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OuterResponseErrorData.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Resource.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <date/tz.h>
#include <thread>


TEST_P(ErpWorkflowTestP, TaskSearchStatus) // NOLINT
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
    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId2, kvnr, secret2, lastModifiedDate, communications2));
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
        ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, lastModifiedDate, communications));
        if(i % 2 == 0)
        {
            ASSERT_NO_FATAL_FAILURE(
                taskAbort(*prescriptionId, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), accessCode, secret));
            ASSERT_NO_FATAL_FAILURE(
                taskGetId(*prescriptionId, kvnr, accessCode, std::nullopt,
                           HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
        }
    }

    // Cancelled tasks can also be retrieved since requirement A_19027_06
    auto bundle = taskGet(kvnr).value();
    EXPECT_EQ(bundle.getResourcesByType<model::Task>("Task").size(), taskNum);
    EXPECT_EQ(bundle.getTotalSearchMatches(), taskNum);

    A_19027_06.test("Retrieve list of cancelled tasks");
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

TEST_P(ErpWorkflowTestP, TaskGetRevinclude ) // NOLINT
{
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

// Test for https://dth01.ibmgcloud.net/jira/browse/ERP-5387
TEST_P(ErpWorkflowTestP, TaskEmptyOutput)// NOLINT
{
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

TEST_P(ErpWorkflowTestP, TaskSearch_AcceptDate_AllPredicates) // NOLINT
{
    const std::vector<std::string> authored_on_dates
    {
        "2023-09-26",// 2023-10-24  01
        "2023-09-27",// 2023-10-25  02
        "2023-09-28",// 2023-10-26  03

        "2024-01-01",// 2024-01-29  04
        "2024-01-02",// 2024-01-30  05
        "2024-01-03",// 2024-01-31  06

        "2024-01-04",// 2024-02-01  07
        "2024-01-05",// 2024-02-02  08

        "2024-07-05",// 2024-08-02  09
        "2024-07-06",// 2024-08-03  10
        "2024-07-07",// 2024-08-04  11

        "2025-01-29",// 2025-02-26  12
        "2025-01-30",// 2025-02-27  13
        "2025-01-31",// 2025-02-28  14

        "2025-02-01",// 2025-03-01  15
        "2025-02-02",// 2025-03-02  16
        "2025-02-03",// 2025-03-03  17
        "2025-02-04",// 2025-03-04  18
        "2025-02-05",// 2025-03-05  19
        "2025-02-06" // 2025-03-06  20
    };

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    EXPECT_FALSE(kvnr.empty());

    if (GetParam() == model::PrescriptionType::apothekenpflichigeArzneimittel)
    {
        for (const std::string& authoredOnDate : authored_on_dates)
        {
            std::optional<model::Task> task;
            ASSERT_NO_FATAL_FAILURE(task = taskCreate(GetParam()));
            ASSERT_TRUE(task.has_value());
            model::Timestamp authoredOn = model::Timestamp::fromGermanDate(authoredOnDate);
            auto prescriptionId = task->prescriptionId();
            const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
            auto bundleXml = kbvBundleXml({
                .prescriptionId = prescriptionId,
                .authoredOn = authoredOn,
                .kvnr = kvnr,
                .kbvVersion = kbvVersion,
            });
            mActivateTaskRequestArgs.withOverrideExpectedKbvVersion(kbvVersion.renderVersion());
            ASSERT_NO_FATAL_FAILURE(taskActivate(prescriptionId, task->accessCode(), toCadesBesSignature(bundleXml, authoredOn)));
        }

        EXPECT_EQ(taskGet(kvnr)->getResourceCount(), 20);

        // eq YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2026"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2025"))->getResourceCount(), 9);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2023"))->getResourceCount(), 3);

        // ne YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2026"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2025"))->getResourceCount(), 11);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2023"))->getResourceCount(), 17);

        // lt YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2026"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2025"))->getResourceCount(), 11);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2024"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2023"))->getResourceCount(), 0);

        // le YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2026"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2025"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2024"))->getResourceCount(), 11);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2023"))->getResourceCount(), 3);

        // gt YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2026"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2025"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2024"))->getResourceCount(), 9);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2023"))->getResourceCount(), 17);

        // ge YYYY
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2026"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2025"))->getResourceCount(), 9);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2024"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2023"))->getResourceCount(), 20);

        // eq YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2026-01"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2025-02"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2023-12"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2024-02"))->getResourceCount(), 2);

        // ne YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2026-01"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2025-03"))->getResourceCount(), 14);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2023-10"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2023-01"))->getResourceCount(), 20);

        // lt YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2025-03"))->getResourceCount(), 14);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2024-01"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2023-10"))->getResourceCount(), 0);

        // le YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2024-12"))->getResourceCount(), 11);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2023-12"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2023-10"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2023-09"))->getResourceCount(), 0);

        // gt YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2025-02"))->getResourceCount(), 6);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2025-03"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2023-12"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2024-01"))->getResourceCount(), 14);

        // ge YYYY-mm
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2025-02"))->getResourceCount(), 9);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2023-10"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2023-12"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2024-01"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2024-08"))->getResourceCount(), 12);

        // eq YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2026-01-01"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=eq2023-10-25"))->getResourceCount(), 1);

        // ne YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2026-01-01"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ne2024-02-01"))->getResourceCount(), 19);

        // lt YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2023-11-01"))->getResourceCount(), 3);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2023-10-26"))->getResourceCount(), 2);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2023-10-24"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=lt2024-08-02"))->getResourceCount(), 8);

        // le YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2023-09-28"))->getResourceCount(), 0);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2024-02-02"))->getResourceCount(), 8);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=le2026-12-31"))->getResourceCount(), 20);

        // gt YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2023-01-01"))->getResourceCount(), 20);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2023-10-28"))->getResourceCount(), 17);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=gt2023-12-31"))->getResourceCount(), 17);

        // ge YYYY-mm-dd
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2023-10-25"))->getResourceCount(), 19);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2025-03-03"))->getResourceCount(), 4);
        EXPECT_EQ(taskGet(kvnr, UrlHelper::escapeUrl("accept-date=ge2025-03-07"))->getResourceCount(), 0);
    }
}


INSTANTIATE_TEST_SUITE_P(ErpWorkflowTestPInst, ErpWorkflowTestP,
                         testing::ValuesIn(testutils::allPrescriptionTypes()));
