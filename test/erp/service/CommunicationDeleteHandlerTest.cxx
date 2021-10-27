/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/CommunicationDeleteHandler.hxx"

#include "erp/ErpRequirements.hxx"

#include "erp/common/MimeType.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/util/FileHelper.hxx"

#include "mock/crypto/MockCryptography.hxx"

#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test_config.h"

#include "tools/jwt/JwtBuilder.hxx"

using namespace model;

class CommunicationDeleteHandlerTest : public ServerTestBase
{
public:
    CommunicationDeleteHandlerTest() :
        ServerTestBase()
    {
    }
};


TEST_F(CommunicationDeleteHandlerTest, Delete_A_20258)
{
    A_20258.test("E-Rezept-Fachdienst - Communication löschen auf Basis Absender-ID");

    // Add task to database.
    //----------------------

    Task task = addTaskToDatabase({Task::Status::ready, InsurantE});

    std::string_view kvnrTask = task.kvnr().value();
    std::string kvnrRepresentative = InsurantB;

    // Add Communication messages to database.
    //----------------------------------------

    Communication infoReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::InfoReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        "", InfoReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(infoReq.id().value().isValidIheUuid());

    Communication reply = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Reply,
        {ActorRole::Pharmacists, mPharmacy}, {ActorRole::Insurant, std::string(kvnrTask)},
        "", ReplyMessage, model::Timestamp::now() });
    ASSERT_TRUE(reply.id().value().isValidIheUuid());

    Communication dispReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::DispReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        std::string(task.accessCode()), DispReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(dispReq.id().value().isValidIheUuid());

    Communication representative = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Representative,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Representative, kvnrRepresentative},
        std::string(task.accessCode()), RepresentativeMessageByInsurant, model::Timestamp::now() });
    ASSERT_TRUE(representative.id().value().isValidIheUuid());

    // Create a client
    //----------------

    auto client = createClient();

    auto database = createDatabase();

    // Delete InfoReq message
    //-----------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + infoReq.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete Reply message
    //----------------------
    {
        A_20776.test("E-Rezept-Fachdienst - Nachricht durch Abgebenden löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtPharamacy{ mJwtBuilder.makeJwtApotheke(mPharmacy) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + reply.id().value().toString(), jwtPharamacy), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtPharamacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete DispReq message
    //-----------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + dispReq.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete Representative message
    //------------------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + representative.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }
}

TEST_F(CommunicationDeleteHandlerTest, Delete_A_20259)
{
    A_20259.test("E-Rezept-Fachdienst - Communication löschen mit Warnung wenn vom Empfänger bereits abgerufen");

    // Add task to database.
    //----------------------

    Task task = addTaskToDatabase({ Task::Status::ready, InsurantE });

    std::string_view kvnrTask = task.kvnr().value();
    std::string kvnrRepresentative = InsurantB;

    // Add Communication messages to database.
    //----------------------------------------

    Timestamp timestampSent = Timestamp::now();
    Timestamp timestampReceived = timestampSent + std::chrono::duration<int64_t>(60000);

    Communication infoReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::InfoReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        "", InfoReqMessage,
        timestampSent, timestampReceived });
    ASSERT_TRUE(infoReq.id().value().isValidIheUuid());

    Communication reply = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Reply,
        {ActorRole::Pharmacists, mPharmacy}, {ActorRole::Insurant, std::string(kvnrTask)},
        "", ReplyMessage,
        timestampSent, timestampReceived });
    ASSERT_TRUE(reply.id().value().isValidIheUuid());

    Communication dispReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::DispReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        std::string(task.accessCode()), DispReqMessage,
        timestampSent, timestampReceived });
    ASSERT_TRUE(dispReq.id().value().isValidIheUuid());

    Communication representative = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Representative,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Representative, kvnrRepresentative},
        std::string(task.accessCode()), RepresentativeMessageByInsurant,
        timestampSent, timestampReceived });
    ASSERT_TRUE(representative.id().value().isValidIheUuid());

    // Create a client
    //----------------

    auto client = createClient();

    auto database = createDatabase();

    // Delete InfoReq message
    //-----------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + infoReq.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_TRUE(innerResponse.getHeader().header(Header::Warning).value().find(timestampReceived.toXsDateTime()) != std::string::npos);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete Reply message
    //----------------------
    {
        A_20776.test("E-Rezept-Fachdienst - Nachricht durch Abgebenden löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + reply.id().value().toString(), jwtPharmacy), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_TRUE(innerResponse.getHeader().header(Header::Warning).value().find(timestampReceived.toXsDateTime()) != std::string::npos);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete DispReq message
    //-----------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + dispReq.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_TRUE(innerResponse.getHeader().header(Header::Warning).value().find(timestampReceived.toXsDateTime()) != std::string::npos);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }

    // Delete Representative message
    //------------------------------
    {
        A_20260.test("Anwendungsfall Nachricht durch Versicherten löschen");

        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + representative.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NoContent);
        ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_TRUE(innerResponse.getHeader().header(Header::Warning).value().find(timestampReceived.toXsDateTime()) != std::string::npos);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that the communication no longer exist in the database.
        ASSERT_EQ(countCurr, countPrev - 1);
    }
}

TEST_F(CommunicationDeleteHandlerTest, Delete_InvalidId)
{
    // Add task to database.
    //----------------------

    Task task = addTaskToDatabase({ Task::Status::ready, InsurantE });

    std::string_view kvnrTask = task.kvnr().value();
    std::string kvnrRepresentative = InsurantB;

    Uuid invalidId;

    // Add Communication messages to database.
    //----------------------------------------

    Communication infoReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::InfoReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        "", InfoReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(infoReq.id().value().isValidIheUuid());

    Communication reply = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Reply,
        {ActorRole::Pharmacists, mPharmacy}, {ActorRole::Insurant, std::string(kvnrTask)},
        "", ReplyMessage, model::Timestamp::now() });
    ASSERT_TRUE(reply.id().value().isValidIheUuid());

    Communication dispReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::DispReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        std::string(task.accessCode()), DispReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(dispReq.id().value().isValidIheUuid());

    Communication representative = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Representative,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Representative, kvnrRepresentative},
        std::string(task.accessCode()), RepresentativeMessageByInsurant, model::Timestamp::now() });
    ASSERT_TRUE(representative.id().value().isValidIheUuid());

    // Create a client
    //----------------

    auto client = createClient();

    auto database = createDatabase();

    // Delete InfoReq message
    //-----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + invalidId.toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete Reply message
    //----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + invalidId.toString(), jwtPharmacy), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete DispReq message
    //-----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + invalidId.toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete Representative message
    //------------------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + invalidId.toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }
}

TEST_F(CommunicationDeleteHandlerTest, Delete_InvalidSender)
{
    // Add task to database.
    //----------------------

    Task task = addTaskToDatabase({ Task::Status::ready, InsurantE });

    std::string_view kvnrTask = task.kvnr().value();
    std::string kvnrRepresentative = InsurantB;

    // Add Communication messages to database.
    //----------------------------------------

    Communication infoReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::InfoReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        "", InfoReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(infoReq.id().value().isValidIheUuid());

    Communication reply = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Reply,
        {ActorRole::Pharmacists, mPharmacy}, {ActorRole::Insurant, std::string(kvnrTask)},
        "", ReplyMessage, model::Timestamp::now() });
    ASSERT_TRUE(reply.id().value().isValidIheUuid());

    Communication dispReq = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::DispReq,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Pharmacists, mPharmacy},
        std::string(task.accessCode()), DispReqMessage, model::Timestamp::now() });
    ASSERT_TRUE(dispReq.id().value().isValidIheUuid());

    Communication representative = addCommunicationToDatabase({
        task.prescriptionId(), Communication::MessageType::Representative,
        {ActorRole::Insurant, std::string(kvnrTask)}, {ActorRole::Representative, kvnrRepresentative},
        std::string(task.accessCode()), RepresentativeMessageByInsurant, model::Timestamp::now() });
    ASSERT_TRUE(representative.id().value().isValidIheUuid());

    // Create a client
    //----------------

    auto client = createClient();

    auto database = createDatabase();

    // Delete InfoReq message
    //-----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Pharmacy is not allowed to delete InfoReq sent by the insurant.
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + infoReq.id().value().toString(), jwtPharmacy), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete Reply message
    //----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Insurant is not allowed to delete Reply sent by pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(kvnrTask)) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + reply.id().value().toString(), jwtInsurant), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete DispReq message
    //-----------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Pharmacy is not allowed to delete DispReq sent by the insurant.
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + dispReq.id().value().toString(), jwtPharmacy), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }

    // Delete Representative message
    //------------------------------
    {
        uint64_t countPrev = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Representative is not allowed to delete message sent by insurant.
        const JWT jwtRepresentative{ mJwtBuilder.makeJwtVersicherter(kvnrRepresentative) };
        // Create the inner request
        ClientRequest request(createDeleteHeader("/Communication/" + representative.id().value().toString(), jwtRepresentative), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtRepresentative));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);

        uint64_t countCurr = database->retrieveCommunicationIds(mPharmacy).size()
            + database->retrieveCommunicationIds(std::string(kvnrTask)).size()
            + database->retrieveCommunicationIds(kvnrRepresentative).size();

        // Verify that no communication object has been removed from the database.
        ASSERT_EQ(countCurr, countPrev);
    }
}
