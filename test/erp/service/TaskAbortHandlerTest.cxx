/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/database/Database.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "test/util/JwtBuilder.hxx"

#include "test/erp/model/CommunicationTest.hxx"
#include "test/util/ServerTestBaseAutoCleanup.hxx"


class TaskAbortHandlerTest : public ServerTestBaseAutoCleanup
{
public:
    TaskAbortHandlerTest (void)
        : ServerTestBaseAutoCleanup()
    {
    }

protected:
    void checkCreatedData(
        const model::PrescriptionId& prescriptionId,
        const std::string& communicationSender,
        const std::vector<Uuid>& communicationIds);
    void checkResultingData(
        const model::PrescriptionId& prescriptionId,
        const std::string& communicationSender,
        const std::vector<Uuid>& communicationIds);
};


//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TaskAbortHandlerTest::checkCreatedData(
    const model::PrescriptionId& prescriptionId,
    const std::string& communicationRecipient,
    const std::vector<Uuid>& communicationIds)
{
    auto database = createDatabase();

    std::optional<Database::TaskAndKey> taskWithAccessCode;
    std::optional<model::Binary> healthcareProviderPrescription;

    // The query "retrieveTaskAndPrescription" doesn't include the secret column in the result set.
    std::tie(taskWithAccessCode, healthcareProviderPrescription) =
        database->retrieveTaskAndPrescription(prescriptionId);

    ASSERT_TRUE(taskWithAccessCode.has_value());
    EXPECT_TRUE(taskWithAccessCode->task.kvnr().has_value());
    ASSERT_NO_THROW((void)taskWithAccessCode->task.accessCode());
    EXPECT_TRUE(healthcareProviderPrescription.has_value());

    model::Task::Status taskStatus = taskWithAccessCode->task.status();

    if (taskStatus == model::Task::Status::inprogress || taskStatus == model::Task::Status::completed)
    {
        std::optional<model::Task> taskWithSecret;
        std::optional<model::Bundle> receipt;

        // The query "retrieveTaskAndReceipt" doesn't include the access_code column in the result set.
        std::tie(taskWithSecret, receipt) = database->retrieveTaskAndReceipt(prescriptionId);

        ASSERT_TRUE(taskWithSecret.has_value());
        ASSERT_EQ(taskWithAccessCode->task.prescriptionId(), taskWithSecret->prescriptionId());

        if (taskStatus == model::Task::Status::completed)
        {
            EXPECT_TRUE(taskWithSecret->secret().has_value());
            EXPECT_TRUE(taskWithSecret->owner().has_value());
            EXPECT_TRUE(receipt.has_value());
            EXPECT_TRUE(taskWithSecret->lastMedicationDispense().has_value());
        }
    }

    const auto ids = database->retrieveCommunicationIds(communicationRecipient);
    const auto num = std::count_if(
        ids.begin(), ids.end(),
        [&communicationIds](auto elem) { return std::find(communicationIds.begin(), communicationIds.end(), elem) != communicationIds.end(); });
    EXPECT_EQ(num, static_cast<signed long>(communicationIds.size()));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void TaskAbortHandlerTest::checkResultingData(
    const model::PrescriptionId& prescriptionId,
    const std::string& communicationRecipient,
    const std::vector<Uuid>& communicationIds)
{
    auto database = createDatabase();

    std::optional<Database::TaskAndKey> taskAndKey1;
    std::optional<model::Binary> healthcareProviderPrescription;
    std::optional<model::Task> task2;
    std::optional<model::Bundle> receipt;

    std::tie(taskAndKey1, healthcareProviderPrescription) = database->retrieveTaskAndPrescription(prescriptionId);
    std::tie(task2, receipt) = database->retrieveTaskAndReceipt(prescriptionId);

    ASSERT_TRUE(taskAndKey1.has_value());
    ASSERT_TRUE(task2.has_value());

    EXPECT_FALSE(healthcareProviderPrescription.has_value());
    EXPECT_FALSE(receipt.has_value());

    ASSERT_EQ(taskAndKey1->task.prescriptionId(), task2->prescriptionId());

    EXPECT_FALSE(taskAndKey1->task.kvnr().has_value());
    EXPECT_FALSE(taskAndKey1->task.secret().has_value());
    EXPECT_FALSE(taskAndKey1->task.owner().has_value());
    EXPECT_FALSE(taskAndKey1->task.lastMedicationDispense().has_value());
    ASSERT_ANY_THROW((void)taskAndKey1->task.accessCode());
    EXPECT_FALSE(taskAndKey1->task.healthCarePrescriptionUuid().has_value());
    EXPECT_FALSE(taskAndKey1->task.patientConfirmationUuid().has_value());
    EXPECT_FALSE(taskAndKey1->task.receiptUuid().has_value());

    const auto ids = database->retrieveCommunicationIds(communicationRecipient);
    const auto num = std::count_if(
        ids.begin(), ids.end(),
        [&communicationIds](auto elem) { return std::find(communicationIds.begin(), communicationIds.end(), elem) != communicationIds.end(); });
    EXPECT_EQ(num, 0);
}


// GEMREQ-start A_19027-06
TEST_F(TaskAbortHandlerTest, deletionOfPersonalData)//NOLINT(readability-function-cognitive-complexity)
{
    A_19027_06.test("Check deletion of personal data and Task related communications");
    const std::string insurant = InsurantF;
    const std::string telematicId = mPharmacy.id();

    // Create Task in database
    auto task = addTaskToDatabase({model::Task::Status::inprogress, insurant, telematicId});

    // Store Task related communication in database
    std::vector<Uuid> communicationIds;
    communicationIds.emplace_back( addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
        {ActorRole::Insurant, task.kvnr().value().id()},
        {ActorRole::Pharmacists, telematicId},
        std::string(task.accessCode()),
        DispReqMessage, model::Timestamp::now() }).id().value());
    communicationIds.emplace_back( addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
        {ActorRole::Insurant, task.kvnr().value().id()},
        {ActorRole::Pharmacists, telematicId},
        std::string(task.accessCode()),
        DispReqMessage, model::Timestamp::now() }).id().value());

    // Check prepared data before abort call
    ASSERT_NO_FATAL_FAILURE(checkCreatedData(task.prescriptionId(), telematicId, communicationIds));

    // Call /Task/<id>/$abort for user pharmacist (Task must not be in progress for users other than pharmacy).
    const JWT jwtPharmacy = mJwtBuilder.makeJwtApotheke(telematicId);
    const auto secret = task.secret();
    ASSERT_TRUE(secret.has_value());
    ClientRequest request(
        createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort/?secret=" + std::string(secret.value()), jwtPharmacy), {});
    auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);

    // Check database data after abort call
    ASSERT_NO_FATAL_FAILURE(checkResultingData(task.prescriptionId(), telematicId, communicationIds));
}
// GEMREQ-end A_19027-06


TEST_F(TaskAbortHandlerTest, deletionOfCommunications)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string insurant = InsurantF;
    const std::string telematicId = mPharmacy.id();

    // Create Task in database
    auto task = addTaskToDatabase({ model::Task::Status::ready, insurant, telematicId });

    // Store Task related communication in database
    std::vector<Uuid> communicationIds;
    communicationIds.emplace_back(addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, task.kvnr().value().id()},
        {ActorRole::Pharmacists, telematicId},
        {},
        InfoReqMessage, model::Timestamp::now() }).id().value());

    // Check prepared data before abort call
    ASSERT_NO_FATAL_FAILURE(checkCreatedData(task.prescriptionId(), telematicId, communicationIds));

    // Call /Task/<id>/$abort for user pharmacist
    const JWT jwtInsurant = mJwtBuilder.makeJwtVersicherter(insurant);
    ClientRequest request(createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtInsurant), {});
    auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);

    // Check database data after abort call
    ASSERT_NO_FATAL_FAILURE(checkResultingData(task.prescriptionId(), telematicId, communicationIds));
}


namespace
{

const model::AuditData& getAuditDataForTask(
    const std::vector<model::AuditData>& auditData,
    const model::PrescriptionId& taskId)
{
    const model::AuditData *result = nullptr;
    for(const auto& elem : auditData)
    {
        if(elem.prescriptionId().has_value() && elem.prescriptionId()->toDatabaseId() == taskId.toDatabaseId())
        {
            Expect(result == nullptr, "Only one AuditData record expected");
            result = &elem;
        }
    }
    return *result;;
}

}

TEST_F(TaskAbortHandlerTest, auditDataFromAccessToken_organization)
{
    const auto kvnr = model::Kvnr{InsurantF};
    const std::string telematicId = mPharmacy.id();

    // Create Task in database
    auto task = addTaskToDatabase({model::Task::Status::inprogress, kvnr.id(), telematicId });

    // Call /Task/<id>/$abort for user pharmacist
    const JWT jwtPharmacy = mJwtBuilder.makeJwtApotheke(telematicId);
    const auto organizationNameClaim = jwtPharmacy.stringForClaim(JWT::organizationNameClaim);
    ASSERT_TRUE(organizationNameClaim.has_value());
    const auto secret = task.secret();
    ASSERT_TRUE(secret.has_value());
    ClientRequest request(
        createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort/?secret=" + std::string(secret.value()), jwtPharmacy), {});
    auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);

    const auto prescriptionId = task.prescriptionId();

    auto database = createDatabase();
    const auto auditEvents = database->retrieveAuditEventData(kvnr, {}, {}, {});
    const auto& auditData = getAuditDataForTask(auditEvents, prescriptionId);
    EXPECT_EQ(auditData.metaData().agentWho(), telematicId);
    EXPECT_EQ(auditData.metaData().agentName(), organizationNameClaim.value());
}

TEST_F(TaskAbortHandlerTest, auditDataFromAccessToken_representative)//NOLINT(readability-function-cognitive-complexity)
{
    const auto kvnr = model::Kvnr{InsurantE};
    const std::string telematicId = mPharmacy.id();

    // Create Task in database
    auto task = addTaskToDatabase({model::Task::Status::completed, kvnr.id(), telematicId });

    // Call /Task/<id>/$abort for user representative
    const JWT jwtRepresentative = mJwtBuilder.makeJwtVersicherter("X987654326");
    const auto idNumberClaim = jwtRepresentative.stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(idNumberClaim.has_value());
    const auto displayName = jwtRepresentative.stringForClaim(JWT::displayNameClaim);
    ASSERT_TRUE(displayName.has_value());

    auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtRepresentative);
    header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
    ClientRequest request(header, {});
    auto outerResponse = createClient().send(encryptRequest(request, jwtRepresentative));
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);

    const auto prescriptionId = task.prescriptionId();
    auto database = createDatabase();

    const auto auditEvents = database->retrieveAuditEventData(kvnr, {}, {}, {});
    const auto& auditData = getAuditDataForTask(auditEvents, prescriptionId);
    EXPECT_EQ(auditData.metaData().agentWho(), idNumberClaim.value());
    EXPECT_EQ(auditData.metaData().agentName(), displayName.value());
}


TEST_F(TaskAbortHandlerTest, auditDataFromAccessToken_person)
{
    const auto kvnr = model::Kvnr{InsurantE};
    const std::string telematicId = mPharmacy.id();

    // Create Task in database
    auto task = addTaskToDatabase({model::Task::Status::completed, kvnr.id(), telematicId });

    // Call /Task/<id>/$abort for user insurant
    const JWT jwtInsurant = mJwtBuilder.makeJwtVersicherter(kvnr);

    auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtInsurant);
    header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
    ClientRequest request(header, {});
    auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);

    const auto prescriptionId = task.prescriptionId();
    auto database = createDatabase();

    const auto auditEvents = database->retrieveAuditEventData(kvnr, {}, {}, {});
    const auto& auditData = getAuditDataForTask(auditEvents, prescriptionId);

    // not contained if patient was the issuer
    EXPECT_FALSE(auditData.metaData().agentWho().has_value());
    EXPECT_FALSE(auditData.metaData().agentName().has_value());
}

TEST_F(TaskAbortHandlerTest, checkAccessValidity)
{
    {
        // Create Task in database
        auto task = addTaskToDatabase({model::Task::Status::completed, InsurantE});

        // Call /Task/<id>/$abort for doctor
        const JWT jwtArzt = mJwtBuilder.makeJwtArzt();
        auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtArzt);
        header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
        ClientRequest request(header, {});
        auto outerResponse = createClient().send(encryptRequest(request, jwtArzt));
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Forbidden);
    }

    {
        // Create Task in database
        auto task = addTaskToDatabase({model::Task::Status::draft, InsurantE});

        // Call /Task/<id>/$abort for doctor
        const JWT jwtArzt = mJwtBuilder.makeJwtArzt();
        auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtArzt);
        header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
        ClientRequest request(header, {});
        auto outerResponse = createClient().send(encryptRequest(request, jwtArzt));
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Forbidden);
    }

    {
        // Create Task in database
        auto task = addTaskToDatabase({model::Task::Status::ready, InsurantE});

        // Call /Task/<id>/$abort for doctor
        const JWT jwtArzt = mJwtBuilder.makeJwtArzt();
        auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtArzt);
        header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
        ClientRequest request(header, {});
        auto outerResponse = createClient().send(encryptRequest(request, jwtArzt));
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
    }

    {
        // Create Task in database
        auto task = addTaskToDatabase({model::Task::Status::draft, InsurantE});

        // Call /Task/<id>/$abort for patient
        const JWT jwtInsurant = mJwtBuilder.makeJwtVersicherter(InsurantE);
        auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtInsurant);
        header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
        ClientRequest request(header, {});
        auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Forbidden);
    }

    {
        // Create Task in database
        auto task = addTaskToDatabase({model::Task::Status::completed, InsurantE});

        // Call /Task/<id>/$abort for patient
        const JWT jwtInsurant = mJwtBuilder.makeJwtVersicherter(InsurantE);
        auto header = createPostHeader("/Task/" + task.prescriptionId().toString() + "/$abort", jwtInsurant);
        header.addHeaderField("X-AccessCode", std::string(task.accessCode()));
        ClientRequest request(header, {});
        auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
    }
}
