/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/erp/database/PostgresDatabaseCommunicationTest.hxx"

#include "erp/model/Task.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include "test/util/JsonTestUtils.hxx"

using namespace model;

PostgresDatabaseCommunicationTest::PostgresDatabaseCommunicationTest() :
    PostgresDatabaseTest()
{
}

void PostgresDatabaseCommunicationTest::cleanup()
{
    if (usePostgres())
    {
        clearTables();
    }
}

PrescriptionId PostgresDatabaseCommunicationTest::insertTask(Task& task)
{
    PrescriptionId prescriptionId = database().storeTask(task);
    database().commitTransaction();
    return prescriptionId;
}

void PostgresDatabaseCommunicationTest::deleteTaskByPrescriptionId(const int64_t prescriptionId)
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction.exec("DELETE FROM erp.task WHERE prescription_id = " + std::to_string(prescriptionId));
    transaction.commit();
}

std::optional<Uuid> PostgresDatabaseCommunicationTest::insertCommunication(Communication& communication)
{
    std::optional<Uuid> communicationId = database().insertCommunication(communication);
    database().commitTransaction();
    return communicationId;
}

void PostgresDatabaseCommunicationTest::deleteCommunication(const Uuid& communicationId, const std::string& sender)
{
    database().deleteCommunication(communicationId, sender);
    database().commitTransaction();
}

uint64_t PostgresDatabaseCommunicationTest::countCommunications()
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction.exec("SELECT COUNT(*) FROM erp.communication");
    transaction.commit();
    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    if (!result.empty())
    {
        result.front().at(0).to(count);
    }
    return count;
}

/**
 * Retrieve the 'communication' object from the database with the given id.
 */
std::optional<Communication>
PostgresDatabaseCommunicationTest::retrieveCommunication(const Uuid& communicationId, const std::string_view& sender)
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction.exec(
        "SELECT DISTINCT EXTRACT(EPOCH FROM received), sender_blob_id, message_for_sender, a.salt "
        "FROM erp.communication "
        "LEFT JOIN erp.account a ON a.account_id = sender AND a.blob_id = sender_blob_id "
        "WHERE id = '" + communicationId.toString() + "'");
    transaction.commit();
    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        Expect(result.at(0).size() == 4,
               "Expected four columns in result row (received, blob_id, message and salt).");
        auto blobId = gsl::narrow<BlobId>(result[0][1].as<int32_t>());
        db_model::Blob salt{result[0][3].as<pqxx::binarystring>()};
        auto key = getKeyDerivation().communicationKey(sender, getKeyDerivation().hashIdentity(sender), blobId, salt);
        auto communicationJson = getDBCodec().decode(
            db_model::EncryptedBlob{result[0][2].as<pqxx::binarystring>()}, key);
        Communication communication = Communication::fromJson(communicationJson);
        // Please note that the communication id is not stored in the json string of the message
        // column as the id is only available after the data row has been inserted in the table.
        // To return a valid communication object the id will be set here.
        // Same applies to the time the communication has been received by the recipient.
        if (!result.at(0).at(0).is_null())
        {
            communication.setTimeReceived(Timestamp(result.at(0).at(0).as<double>()));
        }
        communication.setId(communicationId);
        return {std::move(communication)};
    }
    return {};
}

void PostgresDatabaseCommunicationTest::verifyDatabaseIsTidy()
{
    ASSERT_TRUE(database().retrieveCommunications(InsurantA, {}, {}).empty());
    ASSERT_TRUE(database().retrieveCommunications(InsurantB, {}, {}).empty());
    ASSERT_TRUE(database().retrieveCommunications(mPharmacy, {}, {}).empty());
    database().commitTransaction();
}

UrlArguments PostgresDatabaseCommunicationTest::searchForSent (const std::string& sentString)
{
    auto search = UrlArguments({{"sent", "erp.timestamp_from_suuid(id)", SearchParameter::Type::Date}});
    auto request = ServerRequest(Header());
    request.setQueryParameters({{"sent", sentString}});
    search.parse(request, getKeyDerivation());
    return search;
}

UrlArguments PostgresDatabaseCommunicationTest::searchForReceived (const std::string& receivedString)
{
    auto search = UrlArguments({{"received", SearchParameter::Type::Date}});
    auto request = ServerRequest(Header());
    request.setQueryParameters({{"received", receivedString}});
    search.parse(request, getKeyDerivation());
    return search;
}


TEST_F(PostgresDatabaseCommunicationTest, insertCommunicationInfoReq)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Insert object into database.
    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("Do you have the medication available?").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    // The sender and timestamp json objects are usually added by the Post Communication Handler.
    // Those two json objects are mandatory when adding the communication object to the database.
    // So we simulate the Post Communication Handler here:
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::now());
    std::optional<Uuid> id = insertCommunication(c1);

    // Verify that the Communication object has been written to the database.
    ASSERT_EQ(database().retrieveCommunications(mPharmacy, {}, {}).size(), 1);
    database().commitTransaction();

    // Verify that the id field of the Communication object has been initialized.
    ASSERT_EQ(c1.id(), id);
}


TEST_F(PostgresDatabaseCommunicationTest, insertCommunicationReply)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Insert object into database.
    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Insurant, std::string(kvnrInsurant))
        .setPayload("Yes. The medication is available.").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    // The sender and timestamp json objects are usually added by the Post Communication Handler.
    // Those two json objects are mandatory when adding the communication object to the database.
    // So we simulate the Post Communication Handler here:
    c1.setSender(mPharmacy);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> id = insertCommunication(c1);

    // Verify that the Communication object has been writen to the database.
    ASSERT_EQ(database().retrieveCommunications(std::string(kvnrInsurant), {}, {}).size(), 1);
    database().commitTransaction();

    // Verify that the id field of the Communication object has been initialized.
    ASSERT_EQ(c1.id(), id);
}


TEST_F(PostgresDatabaseCommunicationTest, insertCommunicationDispReq)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Insert object into database.
    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("I want to pick up the medication.").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    // The sender and timestamp json objects are usually added by the Post Communication Handler.
    // Those two json objects are mandatory when adding the communication object to the database.
    // So we simulate the Post Communication Handler here:
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> id = insertCommunication(c1);

    // Verify that the Communication object has been writen to the database.
    ASSERT_EQ(database().retrieveCommunications(mPharmacy, {}, {}).size(), 1);
    database().commitTransaction();

    // Verify that the id field of the Communication object has been initialized.
    ASSERT_EQ(c1.id(), id);
}


TEST_F(PostgresDatabaseCommunicationTest, insertCommunicationRepresentative)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Insert object into database.
    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    // The sender and timestamp json objects are usually added by the Post Communication Handler.
    // Those two json objects are mandatory when adding the communication object to the database.
    // So we simulate the Post Communication Handler here:
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> id = insertCommunication(c1);

    // Verify that the Communication object has been writen to the database.
    ASSERT_EQ(database().retrieveCommunications(InsurantA, {}, {}).size(), 1);
    database().commitTransaction();

    // Verify that the id field of the Communication object has been initialized.
    ASSERT_EQ(c1.id(), id);
}


TEST_F(PostgresDatabaseCommunicationTest, deleteCommunication)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Count currently availabe communication objects.
    uint64_t communicationsCountPrev = countCommunications();

    // Insert objects into database.
    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("Do you have the medication available?").createJsonString();
    Communication infoReq = Communication::fromJson(jsonString);
    infoReq.setSender(kvnrInsurant);
    infoReq.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> idInfoReq = insertCommunication(infoReq);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Insurant, std::string(kvnrInsurant))
        .setPayload("Yes. The medication is available.").createJsonString();
    Communication reply = Communication::fromJson(jsonString);
    reply.setSender(mPharmacy);
    reply.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00.000+00:00"));
    std::optional<Uuid> idReply = insertCommunication(reply);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("I want to pick up the medication").createJsonString();
    Communication dispReq = Communication::fromJson(jsonString);
    dispReq.setSender(kvnrInsurant);
    dispReq.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:54:00.000+00:00"));
    std::optional<Uuid> idDispReq = insertCommunication(dispReq);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantB)
        .setPayload("Can you pick up the medication for me?").createJsonString();
    Communication representative = Communication::fromJson(jsonString);
    representative.setSender(kvnrInsurant);
    representative.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:58:00.000+00:00"));
    representative.setTimeReceived(Timestamp::fromXsDateTime("2022-01-24T12:58:00.000+00:00"));
    std::optional<Uuid> idRepresentative = insertCommunication(representative);

    // Verify that the id fields of the Communication objects have been initialized.
    ASSERT_TRUE(idInfoReq.has_value());
    ASSERT_EQ(infoReq.id(), idInfoReq.value());
    ASSERT_TRUE(idReply.has_value());
    ASSERT_EQ(reply.id(), idReply.value());
    ASSERT_TRUE(idDispReq.has_value());
    ASSERT_EQ(dispReq.id(), idDispReq.value());
    ASSERT_TRUE(idRepresentative.has_value());
    ASSERT_EQ(representative.id(), idRepresentative.value());

    // Count currently availabe communication objects. 4 new rows must have been added.
    uint64_t communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev + 4);

    // Retrieve communication objects by their ids.
    std::optional<Communication> infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());
    std::optional<Communication> replyInserted = retrieveCommunication(idReply.value(), reply.sender().value());
    std::optional<Communication> dispReqInserted = retrieveCommunication(idDispReq.value(), dispReq.sender().value());
    std::optional<Communication> representativeInserted =
            retrieveCommunication(idRepresentative.value(), representative.sender().value());

    // Verify that the objects have been stored in the database.
    ASSERT_TRUE(infoReqInserted.has_value());
    ASSERT_EQ(infoReqInserted.value().id(), idInfoReq.value());
    ASSERT_TRUE(replyInserted.has_value());
    ASSERT_EQ(replyInserted.value().id(), idReply.value());
    ASSERT_TRUE(dispReqInserted.has_value());
    ASSERT_EQ(dispReqInserted.value().id(), idDispReq.value());
    ASSERT_TRUE(representativeInserted.has_value());
    ASSERT_EQ(representativeInserted.value().id(), idRepresentative.value());

    // Delete the communication objects by their ids and sender.
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultInfoReqDelete =
        database().deleteCommunication(idInfoReq.value(), std::string(infoReq.sender().value()));
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultReplyDelete =
        database().deleteCommunication(idReply.value(), std::string(reply.sender().value()));
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultDispReqDelete =
        database().deleteCommunication(idDispReq.value(), std::string(dispReq.sender().value()));
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultRepresentativeDelete =
        database().deleteCommunication(idRepresentative.value(), std::string(representative.sender().value()));
    database().commitTransaction();

    // Check the results from the delete requests.

    // InfoReq must have Id but no received time.
    ASSERT_TRUE(std::get<0>(resultInfoReqDelete).has_value());
    ASSERT_EQ(std::get<0>(resultInfoReqDelete).value(), idInfoReq.value());
    ASSERT_FALSE(std::get<1>(resultInfoReqDelete).has_value());

    // Reply must have Id but no received time.
    ASSERT_TRUE(std::get<0>(resultReplyDelete).has_value());
    ASSERT_EQ(std::get<0>(resultReplyDelete).value(), idReply.value());
    ASSERT_FALSE(std::get<1>(resultReplyDelete).has_value());

    // DispReq must have Id but no received time.
    ASSERT_TRUE(std::get<0>(resultDispReqDelete).has_value());
    ASSERT_EQ(std::get<0>(resultDispReqDelete).value(), idDispReq.value());
    ASSERT_FALSE(std::get<1>(resultDispReqDelete).has_value());

    // Representative must have Id AND received time.
    ASSERT_TRUE(std::get<0>(resultRepresentativeDelete).has_value());
    ASSERT_EQ(std::get<0>(resultRepresentativeDelete).value(), idRepresentative.value());
    ASSERT_TRUE(std::get<1>(resultRepresentativeDelete).has_value());
    ASSERT_EQ(std::get<1>(resultRepresentativeDelete).value().toXsDateTime(), "2022-01-24T12:58:00.000+00:00");

    // Check whether the communication objects have been deleted.
    infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());
    replyInserted = retrieveCommunication(idReply.value(), reply.sender().value());
    dispReqInserted = retrieveCommunication(idDispReq.value(), dispReq.sender().value());
    representativeInserted = retrieveCommunication(idRepresentative.value(), representative.sender().value());
    ASSERT_FALSE(infoReqInserted.has_value());
    ASSERT_FALSE(replyInserted.has_value());
    ASSERT_FALSE(dispReqInserted.has_value());
    ASSERT_FALSE(representativeInserted.has_value());

    // Count currently availabe communication objects. The 4 new rows must have been deleted again.
    communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev);
}


TEST_F(PostgresDatabaseCommunicationTest, deleteCommunication_InvalidId)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    Uuid invalidId;

    // Count currently availabe communication objects.
    uint64_t communicationsCountPrev = countCommunications();

    // Insert objects into database.
    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("Do you have the medication available?").createJsonString();
    Communication infoReq = Communication::fromJson(jsonString);
    infoReq.setSender(kvnrInsurant);
    infoReq.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> idInfoReq = insertCommunication(infoReq);

    // Verify that the id fields of the Communication objects have been initialized.
    ASSERT_TRUE(idInfoReq.has_value());
    ASSERT_EQ(infoReq.id(), idInfoReq.value());

    // Count currently availabe communication objects. One new row must have been added.
    uint64_t communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev + 1);

    // Retrieve communication objects by their ids.
    std::optional<Communication> infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());

    // Verify that the objects have been stored in the database.
    ASSERT_TRUE(infoReqInserted.has_value());
    ASSERT_EQ(infoReqInserted.value().id(), idInfoReq.value());

    // Delete the communication object by its id and sender.
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultInfoReqDelete =
        database().deleteCommunication(invalidId, std::string(infoReq.sender().value()));
    database().commitTransaction();

    // Result must be empty.
    ASSERT_FALSE(std::get<0>(resultInfoReqDelete).has_value());
    ASSERT_FALSE(std::get<1>(resultInfoReqDelete).has_value());

    // Check whether the communication objects have been deleted.
    infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());
    ASSERT_TRUE(infoReqInserted.has_value());

    // Count currently availabe communication objects. The new row must not have been deleted.
    communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev + 1);
}


TEST_F(PostgresDatabaseCommunicationTest, deleteCommunication_InvalidSender)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Count currently availabe communication objects.
    uint64_t communicationsCountPrev = countCommunications();

    // Insert objects into database.
    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("Do you have the medication available?").createJsonString();
    Communication infoReq = Communication::fromJson(jsonString);
    infoReq.setSender(kvnrInsurant);
    infoReq.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00.000+00:00"));
    std::optional<Uuid> idInfoReq = insertCommunication(infoReq);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Insurant, std::string(kvnrInsurant))
        .setPayload("Yes. The medication is available.").createJsonString();
    Communication reply = Communication::fromJson(jsonString);
    reply.setSender(mPharmacy);
    reply.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00.000+00:00"));
    std::optional<Uuid> idReply = insertCommunication(reply);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Pharmacists, mPharmacy)
        .setPayload("I want to pick up the medication").createJsonString();
    Communication dispReq = Communication::fromJson(jsonString);
    dispReq.setSender(kvnrInsurant);
    dispReq.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:54:00.000+00:00"));
    std::optional<Uuid> idDispReq = insertCommunication(dispReq);

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantB)
        .setPayload("Can you pick up the medication for me?").createJsonString();
    Communication representative = Communication::fromJson(jsonString);
    representative.setSender(kvnrInsurant);
    representative.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:58:00.000+00:00"));
    std::optional<Uuid> idRepresentative = insertCommunication(representative);

    // Verify that the id fields of the Communication objects have been initialized.
    ASSERT_TRUE(idInfoReq.has_value());
    ASSERT_EQ(infoReq.id(), idInfoReq.value());
    ASSERT_TRUE(idReply.has_value());
    ASSERT_EQ(reply.id(), idReply.value());
    ASSERT_TRUE(idDispReq.has_value());
    ASSERT_EQ(dispReq.id(), idDispReq.value());
    ASSERT_TRUE(idRepresentative.has_value());
    ASSERT_EQ(representative.id(), idRepresentative.value());

    // Count currently availabe communication objects. 4 new rows must have been added.
    uint64_t communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev + 4);

    // Retrieve communication objects by their ids.
    std::optional<Communication> infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());
    std::optional<Communication> replyInserted = retrieveCommunication(idReply.value(), reply.sender().value());
    std::optional<Communication> dispReqInserted = retrieveCommunication(idDispReq.value(), dispReq.sender().value());
    std::optional<Communication> representativeInserted =
        retrieveCommunication(idRepresentative.value(), representative.sender().value());

    // Verify that the objects have been stored in the database.
    ASSERT_TRUE(infoReqInserted.has_value());
    ASSERT_EQ(infoReqInserted.value().id(), idInfoReq.value());
    ASSERT_TRUE(replyInserted.has_value());
    ASSERT_EQ(replyInserted.value().id(), idReply.value());
    ASSERT_TRUE(dispReqInserted.has_value());
    ASSERT_EQ(dispReqInserted.value().id(), idDispReq.value());
    ASSERT_TRUE(representativeInserted.has_value());
    ASSERT_EQ(representativeInserted.value().id(), idRepresentative.value());

    // Delete the communication objects by their ids and sender.
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultInfoReqDelete =
        database().deleteCommunication(idInfoReq.value(), mPharmacy);
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultReplyDelete =
        database().deleteCommunication(idReply.value(), InsurantA);
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultDispReqDelete =
        database().deleteCommunication(idDispReq.value(), mPharmacy);
    std::tuple<std::optional<Uuid>, std::optional<Timestamp>> resultRepresentativeDelete =
        database().deleteCommunication(idRepresentative.value(), mPharmacy);
    database().commitTransaction();

    // Results must be empty.
    ASSERT_FALSE(std::get<0>(resultInfoReqDelete).has_value());
    ASSERT_FALSE(std::get<1>(resultInfoReqDelete).has_value());
    ASSERT_FALSE(std::get<0>(resultReplyDelete).has_value());
    ASSERT_FALSE(std::get<1>(resultReplyDelete).has_value());
    ASSERT_FALSE(std::get<0>(resultDispReqDelete).has_value());
    ASSERT_FALSE(std::get<1>(resultDispReqDelete).has_value());
    ASSERT_FALSE(std::get<0>(resultRepresentativeDelete).has_value());
    ASSERT_FALSE(std::get<1>(resultRepresentativeDelete).has_value());

    // Check whether the communication objects have been deleted.
    infoReqInserted = retrieveCommunication(idInfoReq.value(), infoReq.sender().value());
    replyInserted = retrieveCommunication(idReply.value(), reply.sender().value());
    dispReqInserted = retrieveCommunication(idDispReq.value(), dispReq.sender().value());
    representativeInserted = retrieveCommunication(idRepresentative.value(), representative.sender().value());
    ASSERT_TRUE(infoReqInserted.has_value());
    ASSERT_TRUE(replyInserted.has_value());
    ASSERT_TRUE(dispReqInserted.has_value());
    ASSERT_TRUE(representativeInserted.has_value());

    // Count currently availabe communication objects. The 4 new rows must not have been deleted.
    communicationsCountCurr = countCommunications();
    ASSERT_EQ(communicationsCountCurr, communicationsCountPrev + 4);
}


TEST_F(PostgresDatabaseCommunicationTest, retrieveCommunications_filterOnRecipient)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects.

    const std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(prescriptionId.toString())
            .setAccessCode(std::string(task.accessCode()))
            .setSender(ActorRole::Insurant, std::string(kvnrInsurant))
            .setRecipient(ActorRole::Insurant, InsurantA)
            .setPayload("json blob A")
            .setTimeSent("2022-01-23T12:34:00Z").createJsonString();
    auto c1 = Communication::fromJson(jsonStringC1);
    insertCommunication(c1);

    const std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(prescriptionId.toString())
            .setAccessCode(std::string(task.accessCode()))
            .setSender(ActorRole::Insurant, std::string(kvnrInsurant))
            .setRecipient(ActorRole::Insurant, InsurantA)
            .setTimeSent("2022-01-23T12:45:00Z")
            .setPayload("json blob B").createJsonString();
    auto c2 = Communication::fromJson(jsonStringC2);
    insertCommunication(c2);

    {
        // Verify that we find them all when filtering on the recipient.
        const auto result = database().retrieveCommunications(InsurantA, {}, {});
        ASSERT_EQ(result.size(), 2);
    }

    {
        // Verify that we find no object when searching for a non-existing recipient.
        const auto result = database().retrieveCommunications(InsurantC, {}, {});
        ASSERT_EQ(result.size(), 0);
    }
    database().commitTransaction();
}


TEST_F(PostgresDatabaseCommunicationTest, retrieveCommunications_communicationId)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects.

    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    const auto c1id = insertCommunication(c1);

    std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob B").createJsonString();
    Communication c2 = Communication::fromJson(jsonStringC2);
    c2.setSender(kvnrInsurant);
    c2.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    const auto c2id = insertCommunication(c2);

    {
        // Verify that we find only c1 when filtering with c1's id.
        const auto result = database().retrieveCommunications(InsurantA, c1id, {});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.front().contentString(), "json blob A");
    }

    {
        // Verify that we find only c2 when filtering with c2's id.
        const auto result = database().retrieveCommunications(InsurantA, c2id, {});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.front().contentString(), "json blob B");
    }
    database().commitTransaction();
}


/**
 * This test is work in progress due to the complex nature of FHIR date data type: https://www.hl7.org/fhir/search.html#date
 * At the moment only a part of the underlying data types is (dateTime) is implemented and allow only test
 * for equality. Periods and timing are not supported yet.
*/
TEST_F(PostgresDatabaseCommunicationTest, retrieveCommunications_sent)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects.

    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    insertCommunication(c1);

    std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob B").createJsonString();
    Communication c2 = Communication::fromJson(jsonStringC2);
    c2.setSender(kvnrInsurant);
    c2.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    insertCommunication(c2);

    {
        // Verify that we find only c1 when filtering on its `sent` dateTime.
        const auto result = database().retrieveCommunications(InsurantA, {}, searchForSent("2022-01-23T12:34:00Z"));
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result.front().contentString(), "json blob A");
    }

    {
        // Verify that we find only c2 when filtering on its `sent` dateTime.
        const auto result = database().retrieveCommunications(InsurantA, {}, searchForSent("2022-01-23T12:45:00Z"));
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result.front().contentString(), "json blob B");
    }

    {
        // Verify that we don't find any object when using a non-matching `sent` dateTime.
        const auto result = database().retrieveCommunications(InsurantA, {}, searchForSent("2022-01-23T01:23:00Z"));
        EXPECT_EQ(result.size(), 0);
    }
    database().commitTransaction();
}


/**
 * This test is work in progress due to the complex nature of FHIR date data type: https://www.hl7.org/fhir/search.html#date
 * At the moment only a part of the underlying data types is (dateTime) is implemented and allow only test
 * for equality. Periods and timing are not supported yet.
*/
TEST_F(PostgresDatabaseCommunicationTest, retrieveCommunications_received)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects.

    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    c1.setSender(kvnrInsurant);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    c1.setTimeReceived(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    insertCommunication(c1);

    std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("json blob B").createJsonString();
    Communication c2 = Communication::fromJson(jsonStringC2);
    c2.setSender(kvnrInsurant);
    c2.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    c2.setTimeReceived(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    insertCommunication(c2);

    // Note that we can't filter for c1 because of its missing received timestamp.

    {
        // Verify that we find only c2 when filtering on its `received` dateTime.
        const auto result = database().retrieveCommunications(InsurantA, {}, searchForReceived("2022-01-23T12:45:00Z"));
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result.front().contentString(), "json blob B");
    }

    {
        // Verify that we don't find any object when using a non-matching `received` dateTime.
        const auto result = database().retrieveCommunications(InsurantA, {}, searchForReceived("2022-01-23T01:45:00Z"));
        EXPECT_EQ(result.size(), 0);
    }
    database().commitTransaction();
}


/**
 * This test is work in progress because the task id is not yet a member of the Communication class nor of
 * the erp_communication table. Therefore it can not yet act as filter on the count.
*/
TEST_F(PostgresDatabaseCommunicationTest, countRepresentativeCommunications)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects with three mutual exclusive pairs of senders and receivers.

    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Insurant, std::string(kvnrInsurant))
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    c1.setSender(InsurantB);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    insertCommunication(c1);

    std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantB)
        .setPayload("json blob B").createJsonString();
    Communication c2 = Communication::fromJson(jsonStringC2);
    c2.setSender(kvnrInsurant);
    c2.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    insertCommunication(c2);

    std::string jsonStringC3 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantC)
        .setPayload("json blob C").createJsonString();
    Communication c3 = Communication::fromJson(jsonStringC3);
    c3.setSender(kvnrInsurant);
    c3.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    insertCommunication(c3);

    {
        // Verify that we find 2 communication objects for kvnrInsurant <-> InsurantB.
        const auto count = database().countRepresentativeCommunications(std::string(kvnrInsurant), InsurantB, prescriptionId);
        EXPECT_EQ(count, 2);
    }

    {
        // Verify that we find 2 communication objects for InsurantB <-> kvnrInsurant.
        const auto count = database().countRepresentativeCommunications(InsurantB, std::string(kvnrInsurant), prescriptionId);
        EXPECT_EQ(count, 2);
    }

    {
        // Verify that we find only find 1 communication object for kvnrInsurant <-> InsurantC.
        const auto count = database().countRepresentativeCommunications(std::string(kvnrInsurant), InsurantC, prescriptionId);
        EXPECT_EQ(count, 1);
    }

    {
        // Verify that we find only find 1 communication object for InsurantC <-> kvnrInsurant.
        const auto count = database().countRepresentativeCommunications(InsurantC, std::string(kvnrInsurant), prescriptionId);
        EXPECT_EQ(count, 1);
    }

    {
        // Verify that we don't find any communication object for InsurantB <-> InsurantC.
        const auto count = database().countRepresentativeCommunications(InsurantB, InsurantC, prescriptionId);
        EXPECT_EQ(count, 0);
    }

    {
        // Verify that we don't find any communication object for InsurantC <-> InsurantB.
        const auto count = database().countRepresentativeCommunications(InsurantC, InsurantB, prescriptionId);
        EXPECT_EQ(count, 0);
    }
    database().commitTransaction();
}


TEST_F(PostgresDatabaseCommunicationTest, markCommunicationsAsReceived)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Make sure that there are no left overs from previous tests.
    verifyDatabaseIsTidy();

    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    // Set up a small set of communication objects with three mutual exclusive pairs of senders and receivers.

    std::string jsonStringC1 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setRecipient(ActorRole::Insurant, std::string(kvnrInsurant))
        .setPayload("json blob A").createJsonString();
    Communication c1 = Communication::fromJson(jsonStringC1);
    c1.setSender(InsurantB);
    c1.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    const auto c1id = insertCommunication(c1);

    std::string jsonStringC2 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantB)
        .setPayload("json blob B").createJsonString();
    Communication c2 = Communication::fromJson(jsonStringC2);
    c2.setSender(kvnrInsurant);
    c2.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    c2.setTimeReceived(Timestamp::fromXsDateTime("2022-01-23T12:45:00Z"));
    const auto c2id = insertCommunication(c2);

    std::string jsonStringC3 = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(prescriptionId.toString())
        .setAccessCode(std::string(task.accessCode()))
        .setRecipient(ActorRole::Insurant, InsurantC)
        .setPayload("json blob C").createJsonString();
    Communication c3 = Communication::fromJson(jsonStringC3);
    c3.setSender(kvnrInsurant);
    c3.setTimeSent(Timestamp::fromXsDateTime("2022-01-23T12:34:00Z"));
    const auto c3id = insertCommunication(c3);

    // Mark c1 and c2 as received.
    database().markCommunicationsAsRetrieved(
        {c1id.value(), c2id.value()}, Timestamp::fromXsDateTime("2021-01-24T12:34:56Z"), std::string(kvnrInsurant));
    database().commitTransaction();

    // Verify that c1 has been updated.
    auto expectedC1 = database().retrieveCommunications(std::string(kvnrInsurant), c1id.value(), {});
    ASSERT_EQ(expectedC1.size(), 1);
    EXPECT_EQ(expectedC1.front().contentString(), "json blob A");
    EXPECT_EQ(expectedC1.front().timeReceived().value().toXsDateTime(), "2021-01-24T12:34:56.000+00:00");

    // Verify that c2 has not been updated and still has the previously set value.
    auto expectedC2 = database().retrieveCommunications(InsurantB, c2id.value(), {});
    ASSERT_EQ(expectedC2.size(), 1);
    EXPECT_EQ(expectedC2.front().contentString(), "json blob B");
    EXPECT_EQ(expectedC2.front().timeReceived().value().toXsDateTime(), "2022-01-23T12:45:00.000+00:00");

    // Verify that c3 has not been updated.
    auto expectedC3 = database().retrieveCommunications(InsurantC, c3id.value(), {});
    ASSERT_EQ(expectedC3.size(), 1);
    EXPECT_EQ(expectedC3.front().contentString(), "json blob C");
    EXPECT_FALSE(expectedC3.front().timeReceived().has_value());

    database().commitTransaction();
}


TEST_F(PostgresDatabaseCommunicationTest, deleteCommunicationsForTask)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
    std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
    Task task = Task::fromJson(jsonString);
    PrescriptionId prescriptionId = insertTask(task);
    std::string_view kvnrInsurant = task.kvnr().value();

    {
        jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                .setPrescriptionId(prescriptionId.toString())
                .setAccessCode(std::string(task.accessCode()))
                .setRecipient(ActorRole::Pharmacists, mPharmacy)
                .setPayload("Message text 1.").createJsonString();
        Communication comm = Communication::fromJson(jsonString);
        comm.setSender(kvnrInsurant);
        comm.setTimeSent(Timestamp::fromXsDateTime("2021-02-17T13:34:45.940+00:00"));
        insertCommunication(comm);
    }
    {
        jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                .setPrescriptionId(prescriptionId.toString())
                .setAccessCode(std::string(task.accessCode()))
                .setRecipient(ActorRole::Pharmacists, mPharmacy)
                .setPayload("Message text 1.").createJsonString();
        Communication comm = Communication::fromJson(jsonString);
        comm.setSender(kvnrInsurant);
        comm.setTimeSent(Timestamp::fromXsDateTime("2021-02-17T14:50:11.863+00:00"));
        insertCommunication(comm);
    }
    database().deleteCommunicationsForTask(prescriptionId);
    database().commitTransaction();

    const Query countCommunicationsForTask { "countCommunications",
        "SELECT COUNT(*) FROM erp.communication WHERE prescription_id = $1" };

    prepare (countCommunicationsForTask);

    auto txn2 = createTransaction();
    pqxx::result result;
    ASSERT_NO_THROW(result = txn2.exec_prepared(
        pqxx::zview{ countCommunicationsForTask.name },
        prescriptionId.toDatabaseId()));
    txn2.commit();
    ASSERT_EQ(result.size(), 1);

    ASSERT_EQ(result.front().at(0).as<int>(), 0);
}
