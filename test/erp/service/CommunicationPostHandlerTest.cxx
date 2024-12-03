/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/CommunicationPostHandler.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/OperationOutcome.hxx"

#include "shared/ErpRequirements.hxx"

#include "shared/network/message/MimeType.hxx"
#include "shared/crypto/Certificate.hxx"
#include "erp/model/Binary.hxx"
#include "shared/util/Base64.hxx"

#include "mock/crypto/MockCryptography.hxx"

#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test/util/StaticData.hxx"

#include "test/util/JwtBuilder.hxx"


using namespace model;
using namespace model::resource;

class CommunicationPostHandlerTest : public ServerTestBase
{
public:
    CommunicationPostHandlerTest() :
        ServerTestBase()
    {
    }

    Header createCommunicationPostHeader(
        const std::string& path,
        const std::optional<const JWT>& jwtToken,
        std::optional<std::string> accessCode = {},
        const ContentMimeType& contentMimeType = ContentMimeType::fhirJsonUtf8) const
    {
        Header header = ServerTestBase::createPostHeader(path, jwtToken);
        header.addHeaderField(Header::ContentType, contentMimeType);
        if (accessCode.has_value())
        {
            header.addHeaderField(Header::XAccessCode, accessCode.value());
        }
        return header;
    }
};


TEST_F(CommunicationPostHandlerTest, ValidationErrorOnSenderProfessionOid)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });

    const std::string kvnrInsurant = task.kvnr().value().id();
    std::string kvnrRepresentative = InsurantB;

    auto client = createClient();

    // Pharmacy sends requests with invalid profile
    //---------------------------------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy.id()) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(kvnrInsurant) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy.id()) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(kvnrInsurant) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrInsurant)
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy.id()) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrInsurant)
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(kvnrInsurant) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }

    // Insurant sends requests with invalid profile
    //---------------------------------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrRepresentative)
            .setPayload(DispReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrRepresentative)
            .setPayload(DispReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(mPharmacy.id()) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
}

TEST_F(CommunicationPostHandlerTest, ValidationErrorOnSender)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });

    const std::string kvnrInsurant = task.kvnr().value().id();
    std::string kvnrRepresentative = InsurantB;

    auto client = createClient();

    // Sender is KVNR of insurant with wrong format
    //---------------------------------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(mPharmacy.id()) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(DispReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(mPharmacy.id()) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrInsurant)
            .setPayload(RepresentativeMessageByRepresentative).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(mPharmacy.id()) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }

    // Sender is Telematic ID of pharmacy with wrong format
    //-----------------------------------------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrRepresentative)
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(kvnrInsurant) }; // using wrong combination on purpose
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
}

TEST_F(CommunicationPostHandlerTest, ValidationErrorOnRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });
    const std::string kvnrInsurant = task.kvnr().value().id();

    auto client = createClient();

    // Wrong format of telematic ID
    //-----------------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, "12345678901234567890")
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, "X123456788")
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, "12345678901234567890")
            .setAccessCode(std::string(task.accessCode()))
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(DispReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, "X123456788")
            .setAccessCode(std::string(task.accessCode()))
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(DispReqMessage).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Wrong format of KVNR
    //---------------------
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, "X1234567")
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy.id()) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, "X1234567")
            .setPayload(ReplyMessage).createJsonString();
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(mPharmacy.id()) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtPharmacy),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, "X12345678801234567890")
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(RepresentativeMessageByInsurant).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, "X12345678801234567890")
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(RepresentativeMessageByInsurant).createJsonString();
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, InfoReq)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({Task::Status::ready, InsurantA, {}, TaskAccessCode});
    const std::string kvnrInsurant = task.kvnr().value().id();

    // Please note that the MimeType "text/plain" is no longer existing.
    // But it doesn't matter as it will only be checked whether the size
    // of the attachment is correctly verified.
    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setPayload(InfoReqMessage).createJsonString();

    // Create a client
    auto client = createClient();

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    // Create the inner request
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    ASSERT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
    ASSERT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), MimeType::binary);
    std::optional<ClientResponse> innerResponse;
    ASSERT_NO_THROW(innerResponse.emplace(mTeeProtocol.parseResponse(outerResponse)));
    // ERP-21119 - How to handle CommunicationInfoReq
    EXPECT_EQ(innerResponse->getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(CommunicationPostHandlerTest, Reply)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });
    const std::string kvnrInsurant = task.kvnr().value().id();
    const JWT jwtPharmacy{mJwtBuilder.makeJwtApotheke()};

    // Please note that the MimeType "text/plain" is no longer existing.
    // But it doesn't matter as it will only be checked whether the size
    // of the attachment is correctly verified.
    std::string jsonString =
        CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrInsurant)
            .setSender(ActorRole::Pharmacists, jwtPharmacy.stringForClaim(JWT::idNumberClaim).value())
            .setPayload(ReplyMessage)
            .createJsonString();

    // Create a client
    auto client = createClient();

    // Create the inner request
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirXmlUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    std::string location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
    Uuid id(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    std::optional<Communication> communication;
    ASSERT_NO_THROW(communication.emplace(
        ResourceFactory<Communication>::fromXml(innerResponse.getBody(), *StaticData::getXmlValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationReply)));
    // The communication id must have been added to the json body.
    ASSERT_TRUE(communication->id().has_value());
    ASSERT_TRUE(communication->id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communication->timeSent().has_value());
    ASSERT_NO_THROW(communication->timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_THROW(communication->sender());
    ASSERT_EQ(model::getIdentityString(*communication->sender()), mPharmacy);
}

TEST_F(CommunicationPostHandlerTest, DispReq)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });
    const std::string kvnrInsurant = task.kvnr().value().id();

    // Please note that the MimeType "text/plain" is no longer existing.
    // But it doesn't matter as it will only be checked whether the size
    // of the attachment is correctly verified.
    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
        .setAccessCode(std::string(task.accessCode()))
        .setPayload(DispReqMessage).createJsonString();

    // Create a client
    auto client = createClient();

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    // Create the inner request
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    std::string location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
    Uuid id(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    std::string bodyResponse;

    ASSERT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1.emplace(
        ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationDispReq)));

    auto& communication = *communication1;

    // The communication id must have been added to the json body.
    ASSERT_TRUE(communication.id().has_value());
    ASSERT_TRUE(communication.id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communication.timeSent().has_value());
    ASSERT_NO_FATAL_FAILURE(communication.timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_FATAL_FAILURE(communication.sender());
    ASSERT_EQ(model::getIdentityString(*communication.sender()), InsurantA);

    {
        // Check that it works with basedOn containing a full Url:
        jsonString = String::replaceAll(jsonString, "Task/", "https://erp.lu2.erezepttest.net:443/Task/");
        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, Representative)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

    const std::string kvnrInsurant = task.kvnr().value().id();
    std::string kvnrRepresentative = InsurantB;

    // Create a client
    auto client = createClient();

    // Insurant asks Representative to get the medication
    //---------------------------------------------------

    // Please note that the MimeType "text/plain" is no longer existing.
    // But it doesn't matter as it will only be checked whether the size
    // of the attachment is correctly verified.
    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Insurant, kvnrRepresentative)
        .setAccessCode(std::string(task.accessCode()))
        .setPayload(RepresentativeMessageByInsurant).createJsonString();

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    // Create the inner request
    ClientRequest requestByInsurant(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(requestByInsurant, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    std::string location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
    Uuid id(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    std::string bodyResponse;

    ASSERT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1.emplace(
        ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationRepresentative)));
    auto& communicationByInsurant = *communication1;

    // The communication id must have been added to the json body.
    ASSERT_TRUE(communicationByInsurant.id().has_value());
    ASSERT_TRUE(communicationByInsurant.id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communicationByInsurant.timeSent().has_value());
    ASSERT_NO_FATAL_FAILURE(communicationByInsurant.timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_FATAL_FAILURE(communicationByInsurant.sender());
    ASSERT_EQ(model::getIdentityString(*communicationByInsurant.sender()), kvnrInsurant);

    // Representative answers insurant
    //--------------------------------

    // Please note that the MimeType "text/plain" is no longer existing.
    // But it doesn't matter as it will only be checked whether the size
    // of the attachment is correctly verified.
    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Insurant, kvnrInsurant)
        .setPayload(RepresentativeMessageByRepresentative).createJsonString();

    const JWT jwtRepresentative{ mJwtBuilder.makeJwtVersicherter(kvnrRepresentative) };
    // Create the inner request
    ClientRequest requestByRepresentative(createCommunicationPostHeader("/Communication", jwtRepresentative, std::string(task.accessCode())), jsonString);

    // Send the request.
    outerResponse = client.send(encryptRequest(requestByRepresentative, jwtRepresentative));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    idString = location.substr(std::string(structure_definition::communicationLocation).length());
    id = Uuid(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    ASSERT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

    std::optional<Communication> communicationOpt;
    ASSERT_NO_THROW(communicationOpt.emplace(
        ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationRepresentative)));

    auto& communicationByRepresentative = *communicationOpt;

    // The communication id must have been added to the json body.
    ASSERT_TRUE(communicationByRepresentative.id().has_value());
    ASSERT_TRUE(communicationByRepresentative.id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communicationByRepresentative.timeSent().has_value());
    ASSERT_NO_FATAL_FAILURE(communicationByRepresentative.timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_FATAL_FAILURE(communicationByRepresentative.sender());
    ASSERT_EQ(model::getIdentityString(*communicationByRepresentative.sender()), kvnrRepresentative);
}

TEST_F(CommunicationPostHandlerTest, InfoReqIncorrectMatches)//NOLINT(readability-function-cognitive-complexity)
{
    auto task1 = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });
    auto task2 = addTaskToDatabase({ Task::Status::ready, InsurantB, {} });
    const std::string kvnrInsurantB = task2.kvnr().value().id();

    auto client = createClient();

    // Insurant sends info request with KVNR in access TOKEN which doesn't match the KVNR in the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Access TOKEN of kvnr of task 2 does not match KVNR in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurantB) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Representative sends info request with access CODE in header which doesn't match the access code of the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Access CODE of task 2 is placed into header which doesn't match access code in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantC) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task2.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, DispReqIncorrectMatches)//NOLINT(readability-function-cognitive-complexity)
{
    auto task1 = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });
    auto task2 = addTaskToDatabase({ Task::Status::ready, InsurantB, {} });
    const std::string kvnrInsurantB = task2.kvnr().value().id();

    auto client = createClient();

    // Insurant sends dispense request with KVNR in access TOKEN which doesn't match the KVNR in the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task1.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Access TOKEN of kvnr of task 2 does not match KVNR in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurantB) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Representative sends dispense request with access CODE in header which doesn't match the access code of the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task1.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Access CODE of task 2 is placed into header which doesn't match access code in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantC) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task2.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, RepresentativeIncorrectMatches)//NOLINT(readability-function-cognitive-complexity)
{
    auto task1 = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });
    auto task2 = addTaskToDatabase({ Task::Status::ready, InsurantB, {} });
    const std::string kvnrInsurantA = task1.kvnr().value().id();
    const std::string kvnrInsurantB = task2.kvnr().value().id();

    auto client = createClient();

    // Insurant sends representative message with KVNR in access TOKEN which doesn't match the KVNR in the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantC)
            .setAccessCode(std::string(task1.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Access TOKEN of kvnr of task 2 does not match KVNR in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurantB) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Representative sends representative message with access CODE in header which doesn't match the access code of the referenced task.
    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task1.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, kvnrInsurantA)
            .setAccessCode(std::string(task1.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Access CODE of task 2 is placed into header which doesn't match access code in referenced task 1.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantC) };
        ClientRequest request(
            createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task2.accessCode())),
            jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, Representative_A_20229)//NOLINT(readability-function-cognitive-complexity)
{
    A_20229_01.test("limit KVNR -> KVNR messages to 10 per task");

    std::vector<Task> tasks;

    tasks.emplace_back(addTaskToDatabase({Task::Status::ready, InsurantA, {}}));
    tasks.emplace_back(addTaskToDatabase({Task::Status::ready, InsurantA, {}}));

    const auto maxMessageCount = Configuration::instance().getIntValue(ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES);

    // Create a client
    auto client = createClient();

    for (const auto& task : tasks)
    {
        const std::string kvnrInsurant = task.kvnr().value().id();
        JWT jwtSender = mJwtBuilder.makeJwtVersicherter(kvnrInsurant);
        JWT jwtRecipient = mJwtBuilder.makeJwtVersicherter(InsurantB);

        std::string messageA = RepresentativeMessageByInsurant;
        std::string messageB = RepresentativeMessageByRepresentative;

        for (int msgCount = 1; msgCount <= maxMessageCount; ++msgCount)
        {
            // The accessCode must be added to the communications resource's basedOn element
            // if the insurant sends the message to the representative.
            // The accessCode must be added to the header if the message is sent by the representative.
            std::optional<std::string> communicationAccessCode;
            std::optional<std::string> headerAccessCode;
            if (jwtSender.stringForClaim(JWT::idNumberClaim).value() == kvnrInsurant)
                communicationAccessCode = task.accessCode();
            else
                headerAccessCode = task.accessCode();

            CommunicationJsonStringBuilder builder(Communication::MessageType::Representative);
            builder.setPrescriptionId(task.prescriptionId().toString());
            builder.setRecipient(ActorRole::Insurant, jwtRecipient.stringForClaim(JWT::idNumberClaim).value());
            builder.setPayload(messageA);
            if (communicationAccessCode.has_value())
                builder.setAccessCode(communicationAccessCode.value());
            std::string jsonString = builder.createJsonString();

            // Create the inner request
            ClientRequest request(createCommunicationPostHeader("/Communication", jwtSender, headerAccessCode), jsonString);

            // Send the request.
            auto outerResponse = client.send(encryptRequest(request, jwtSender));

            // Verify and decrypt the outer response. Also the generic part of the inner response.
            auto innerResponse = verifyOuterResponse(outerResponse);
            EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

            EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

            // The communication id must have been set in the header field "Location"
            std::string location = innerResponse.getHeader().header(Header::Location).value();
            EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
            std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
            Uuid id(idString);
            EXPECT_TRUE(id.isValidIheUuid());

            std::string bodyResponse;

            ASSERT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

            std::optional<Communication> communication1;
            ASSERT_NO_THROW(communication1.emplace(
                ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
                    .getValidated(model::ProfileType::Gem_erxCommunicationRepresentative)));

            auto& communication = *communication1;

            // The communication id must have been added to the json body.
            ASSERT_TRUE(communication.id().has_value());
            ASSERT_TRUE(communication.id()->isValidIheUuid());

            // The time sent must have been added to the json body.
            ASSERT_TRUE(communication.timeSent().has_value());
            ASSERT_NO_FATAL_FAILURE(communication.timeSent());

            // The sender must have been taken from the access token.
            ASSERT_NO_FATAL_FAILURE(communication.sender());
            ASSERT_EQ(model::getIdentityString(*communication.sender()), jwtSender.stringForClaim(JWT::idNumberClaim));

            std::swap(jwtSender, jwtRecipient);
            std::swap(messageA, messageB);
        }

        // Now the 11th message follows which should be rejected.
        //-------------------------------------------------------

        // The accessCode must be added to the communications resource's basedOn element
        // if the insurant sends the message to the representative.
        // The accessCode must be added to the header if the message is sent by the representative.
        std::optional<std::string> communicationAccessCode;
        std::optional<std::string> headerAccessCode;
        if (jwtSender.stringForClaim(JWT::idNumberClaim).value() == kvnrInsurant)
            communicationAccessCode = task.accessCode();
        else
            headerAccessCode = task.accessCode();

        CommunicationJsonStringBuilder builder(Communication::MessageType::Representative);
        builder.setPrescriptionId(task.prescriptionId().toString());
        builder.setRecipient(ActorRole::Insurant, jwtRecipient.stringForClaim(JWT::idNumberClaim).value());
        builder.setPayload(messageA).createJsonString();
        if (communicationAccessCode.has_value())
            builder.setAccessCode(communicationAccessCode.value());
        std::string jsonString = builder.createJsonString();

        // Create the inner request
        ClientRequest request(createCommunicationPostHeader("/Communication", jwtSender, headerAccessCode), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtSender));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::TooManyRequests);
    }
}

TEST_F(CommunicationPostHandlerTest, Representative_A_20230)//NOLINT(readability-function-cognitive-complexity)
{
    A_20230_01.test("restrict transfer for messages following the represantive schema to prescriptions that are ready or in-progress");

    // see "fill_tables_with_static_testvalues.sql"
    std::vector<Task::Status> tasksStatus = {
        Task::Status::draft,
        Task::Status::ready,
        Task::Status::inprogress,
        Task::Status::completed,
        Task::Status::cancelled
    };

    // Create a client
    auto client = createClient();

    for (const auto& taskStatus : tasksStatus)
    {
        std::optional<Task> task;
        const model::Kvnr kvnrInsurant{std::string{InsurantA}, model::Kvnr::Type::gkv};

        if (taskStatus == Task::Status::cancelled)
        {
            // To cancel (abort) a task it must be created before with another status.
            task = addTaskToDatabase({ Task::Status::inprogress, InsurantA, {}, TaskAccessCode });
            abortTask(task.value());
        }
        else
        {
            task = addTaskToDatabase({ taskStatus, InsurantA, {}, TaskAccessCode });
        }

        // For tasks in status draft the kvnr is invalid. We set it here to access them below.
        if (!task->kvnr().has_value())
        {
            task->setKvnr(kvnrInsurant);
        }

        // Please note that the MimeType "text/plain" is no longer existing.
        // But it doesn't matter as it will only be checked whether the size
        // of the attachment is correctly verified.
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task->prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantB)
            .setAccessCode(TaskAccessCode) // don't use std::string(task->accessCode()) here as for a cancelled task the access code is invalid
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        if (taskStatus == Task::Status::ready || taskStatus == Task::Status::inprogress)
        {
            const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
            // Create the inner request
            ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

            // Send the request.
            auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

            // Verify and decrypt the outer response. Also the generic part of the inner response.
            auto innerResponse = verifyOuterResponse(outerResponse);
            EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

            EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

            // The communication id must have been set in the header field "Location"
            std::string location = innerResponse.getHeader().header(Header::Location).value();
            EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
            std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
            Uuid id(idString);
            EXPECT_TRUE(id.isValidIheUuid());

            std::string bodyResponse;

            ASSERT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

            std::optional<Communication> communication1;
            ASSERT_NO_THROW(communication1.emplace(
                ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
                    .getValidated(model::ProfileType::Gem_erxCommunicationRepresentative)));

            auto& communication = *communication1;

            // The communication id must have been added to the json body.
            ASSERT_TRUE(communication.id().has_value());
            ASSERT_TRUE(communication.id()->isValidIheUuid());

            // The time sent must have been added to the json body.
            ASSERT_TRUE(communication.timeSent().has_value());
            ASSERT_NO_FATAL_FAILURE(communication.timeSent());

            // The sender must have been taken from the access token.
            ASSERT_NO_FATAL_FAILURE(communication.sender());
            ASSERT_EQ(model::getIdentityString(communication.sender().value()), jwtInsurant.stringForClaim(JWT::idNumberClaim).value());
        }
        else
        {
            const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
            // Create the inner request
            ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

            // Send the request.
            auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

            // Verify and decrypt the outer response. Also the generic part of the inner response.
            auto innerResponse = verifyOuterResponse(outerResponse);
            ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
        }
    }
}

TEST_F(CommunicationPostHandlerTest, Representative_A_20231)
{
    A_20231.test("prevent messages to self");

    // Create a client
    auto client = createClient();

    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
        .setAccessCode(std::string(task.accessCode()))
        .setPayload(RepresentativeMessageByInsurant).createJsonString();

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
    // Create the inner request
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(CommunicationPostHandlerTest, DispReq_A19450_contentString_exceedsMaxAllowedSize)
{
    A_19450_01.test("malicious code check");

    // Create a client
    auto client = createClient();
    static constexpr std::string_view prefix = R"({"version": 1, "supplyOptionsType": "delivery", "address": [ "xxx)";
    static constexpr std::string_view infix = R"(",")";
    static constexpr std::string_view suffix = R"("]})";

    std::string contentStringFitsMaxSize;
    contentStringFitsMaxSize.reserve(CommunicationPayload::maxPayloadSize);
    contentStringFitsMaxSize.append(prefix);
    while (contentStringFitsMaxSize.size() < CommunicationPayload::maxPayloadSize - suffix.size())
    {
        size_t rest = CommunicationPayload::maxPayloadSize - contentStringFitsMaxSize.size() - suffix.size();
        size_t count = std::min(500ul, rest - infix.size());
        contentStringFitsMaxSize.append(infix);
        contentStringFitsMaxSize.append(count, 'x');
    }
    contentStringFitsMaxSize.append(suffix);
    ASSERT_EQ(contentStringFitsMaxSize.size(), CommunicationPayload::maxPayloadSize);
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                                 .setPrescriptionId(task.prescriptionId().toString())
                                 .setSender(ActorRole::Insurant, task.kvnr().value().id())
                                 .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                 .setAccessCode(std::string{task.accessCode()})
                                 .setPayload(contentStringFitsMaxSize)
                                 .createJsonString();

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
    // Create the inner request
    ClientRequest requestFitsMaxSize(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(requestFitsMaxSize, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Created);

    std::string contentStringExceedsMaxSize(CommunicationPayload::maxPayloadSize + 1, 'x');

    jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                     .setPrescriptionId(task.prescriptionId().toString())
                     .setSender(ActorRole::Insurant, task.kvnr().value().id())
                     .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                     .setAccessCode(std::string{task.accessCode()})
                     .setPayload(contentStringExceedsMaxSize)
                     .createJsonString();

    // Create the inner request
    ClientRequest requestExceedsMaxSize(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    outerResponse = client.send(encryptRequest(requestExceedsMaxSize, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    std::optional<OperationOutcome> outcome;
    ASSERT_NO_FATAL_FAILURE(outcome = OperationOutcome::fromJsonNoValidation(innerResponse.getBody()));
    EXPECT_EQ(outcome.value().issues().size(), 1);
    EXPECT_EQ(outcome.value().issues().at(0).detailsText.value(), "Invalid request body");
    EXPECT_EQ(outcome.value().issues().at(0).diagnostics.value(), "Payload must not exceed 10 KB.");
}

TEST_F(CommunicationPostHandlerTest, DispReq_A19450_contentReference_notAllowed)
{
    A_19450_01.test("malicious code check");

    // Create a client
    auto client = createClient();

    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });
    const JWT jwtInsurant{mJwtBuilder.makeJwtVersicherter(task.kvnr().value())};

    // "contentReference" must be rejected.
    const std::string externalUrl = "https://dth01.ibmgcloud.net/jira/browse/ERP-4012";
    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                                 .setPrescriptionId(task.prescriptionId().toString())
                                 .setSender(ActorRole::Insurant, jwtInsurant.stringForClaim(JWT::idNumberClaim).value())
                                 .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                 .setAccessCode(std::string{task.accessCode()})
                                 .setPayload(externalUrl)
                                 .createJsonString();
    jsonString = String::replaceAll(jsonString, "contentString", "contentReference");

    // Create the inner request
    ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    std::optional<OperationOutcome> outcome;
    ASSERT_NO_FATAL_FAILURE(outcome = OperationOutcome::fromJsonNoValidation(innerResponse.getBody()));
    EXPECT_EQ(outcome.value().issues().size(), 1);
    EXPECT_EQ(outcome.value().issues().at(0).detailsText.value(), "FHIR-Validation error");
    EXPECT_EQ(outcome.value().issues().at(0).diagnostics.value(),
              R"(Communication.payload[0].contentReference: error: )"
              R"----(At most 0 elements expected, but got 1)----"
              R"----( (from profile: http://hl7.org/fhir/StructureDefinition/Communication|4.0.1); )----");
}

TEST_F(CommunicationPostHandlerTest, DispReq_A19450_contentAttachment_notAllowed)
{
    A_19450_01.test("malicious code check");

    // Create a client
    auto client = createClient();

    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

    // "contentAttachment" must be rejected.
    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
                                 .setPrescriptionId(task.prescriptionId().toString())
                                 .setSender(ActorRole::Insurant, InsurantA)
                                 .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                 .setAccessCode(std::string{task.accessCode()})
                                 .setPayload("##ATTACHMENT##")
                                 .createJsonString();

    jsonString = String::replaceAll(jsonString, "contentString", "contentAttachment");
    std::vector<uint8_t> buffer(100, 'x');
    std::string attachmentData = Base64::encode(buffer);
    const std::string attachment = R"(
                {
                    "contentType": ")" + std::string(MimeType::binary) + R"(",
                    "data": ")" + attachmentData + R"("
                })";
    jsonString = String::replaceAll(jsonString, R"("##ATTACHMENT##")", attachment);

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
    // Create the inner request
    ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    std::optional<OperationOutcome> outcome;
    ASSERT_NO_FATAL_FAILURE(outcome = OperationOutcome::fromJsonNoValidation(innerResponse.getBody()));
    EXPECT_EQ(outcome.value().issues().size(), 1);
    EXPECT_EQ(outcome.value().issues().at(0).detailsText.value(), "FHIR-Validation error");
    EXPECT_EQ(outcome.value().issues().at(0).diagnostics.value(),
              R"(Communication.payload[0].contentAttachment: error:)"
              R"---( At most 0 elements expected, but got 1)---"
              R"---( (from profile: http://hl7.org/fhir/StructureDefinition/Communication|4.0.1); )---");
}


TEST_F(CommunicationPostHandlerTest, Representative_A20885_ExaminationOfInsurant)//NOLINT(readability-function-cognitive-complexity)
{
    A_20885_03.test("Examination of insured person and eligibility");

    // Create a client
    auto client = createClient();

    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

    // First check good case. KVNR of referenced task not equal to KVNR of sender but
    // access code of sender's access token equal to access code of "task.identifier".
    //--------------------------------------------------------------------------------

    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

        EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

        // The communication id must have been set in the header field "Location"
        std::string location = innerResponse.getHeader().header(Header::Location).value();
        EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
        std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
        Uuid id(idString);
        EXPECT_TRUE(id.isValidIheUuid());

        std::string bodyResponse;

        EXPECT_NO_FATAL_FAILURE(bodyResponse = canonicalJson(innerResponse.getBody()));

        std::optional<Communication> communication1;
        ASSERT_NO_THROW(communication1.emplace(
            ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
                .getValidated(model::ProfileType::Gem_erxCommunicationRepresentative)));

        auto& communication = *communication1;

        // The communication id must have been added to the json body.
        ASSERT_TRUE(communication.id().has_value());
        EXPECT_TRUE(communication.id()->isValidIheUuid());

        // The time sent must have been added to the json body.
        ASSERT_TRUE(communication.timeSent().has_value());
        EXPECT_NO_FATAL_FAILURE(communication.timeSent());

        // The sender must have been taken from the access token.
        EXPECT_NO_FATAL_FAILURE(communication.sender());
        EXPECT_EQ(model::getIdentityString(communication.sender().value()), jwtInsurant.stringForClaim(JWT::idNumberClaim).value());
    }

    // Second check bad request. KVNR of referenced task not equal to KVNR of sender and
    // access code of sender's access token not equal to access code of "task.identifier".
    //------------------------------------------------------------------------------------

    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        std::string accessCode = "888bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant, accessCode), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    }

    // Third check for missing access code in header
    //----------------------------------------------

    {
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        ClientRequest requestAToB(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(requestAToB, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    }

    // Fourth check for missing access code in payload (baseOn)
    //---------------------------------------------------------

    {
        // Communication request of type representative requires a prescriptionId + access code in basedOn
        // if sent from the insurant to the representative.
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantB)
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
        // Create the inner request
        ClientRequest requestAToB(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(requestAToB, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    }
}

TEST_F(CommunicationPostHandlerTest, Representative_A20752_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    // Create a client
    auto client = createClient();

    A_20752.test("Exclusion of representative communication from or of verification identity");
    //=========================================================================================

    // The e-prescription service MUST reject the setting of a communication resource with the
    // http status code 400 if this contains a combination of KVNR of the eGK test card and
    // KVNR of the insured in the sender and recipient information.

    // Allowed operation (both sender and recipient of communication message are KVNRs of insured).
    //---------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantB)
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Allowed operation (both sender and recipient of communication message are KVNRs of test card).
    //-----------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMax, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, VerificationIdentityKvnrMin)
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (sender is KNVR of insured but recipient is KVNR of test card).
    //--------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, VerificationIdentityKvnrMax)
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (sender is KVNR of test card but recipient is KVNR of insured).
    //--------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMin, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantA)
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(RepresentativeMessageByInsurant).createJsonString();

        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value()) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest,
       DISABLED_InfoReq_A20753_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    // Create a client
    auto client = createClient();

    A_20753.test("Exclusion of representative communication on or by means of verification identity");
    //================================================================================================

    // The e-prescription service MUST refuse any access to e-prescriptions using AccessCode (representative access)
    // with the http status code 400 if the access results in a combination of KVNR of the eGK test card and
    // KVNR of the insured:
    // - Insured persons are not allowed to access test prescriptions.
    // - With the eGK test card it is not allowed to access prescriptions from insured persons.

    // Allowed operation (both owner of task and sender of communication message are KVNRs of insurants).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Allowed operation (both owner of task and sender of communication message are KVNRs of test card).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMin, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMax) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of insured, sender of communication message is KVNR of test card).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMin) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of test card, sender of communication message is KVNR of insured).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMax, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setPayload(InfoReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantA) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, Reply_A20753_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    // Create a client
    auto client = createClient();

    A_20753.test("Exclusion of representative communication on or by means of verification identity");
    //================================================================================================

    // The e-prescription service MUST refuse any access to e-prescriptions using AccessCode (representative access)
    // with the http status code 400 if the access results in a combination of KVNR of the eGK test card and
    // KVNR of the insured:
    // - Insured persons are not allowed to access test prescriptions.
    // - With the eGK test card it is not allowed to access prescriptions from insured persons.

    // Allowed operation (both owner of task and recipient of communication message are KVNRs of insurants).
    //------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });
        // Pharmacy sends message to representative.
        const JWT jwtPharmacy{mJwtBuilder.makeJwtApotheke()};

        std::string jsonString =
            CommunicationJsonStringBuilder(Communication::MessageType::Reply)
                .setPrescriptionId(task.prescriptionId().toString())
                .setSender(ActorRole::Pharmacists, jwtPharmacy.stringForClaim(JWT::idNumberClaim).value())
                .setRecipient(ActorRole::Insurant, InsurantB)
                .setPayload(ReplyMessage)
                .createJsonString();

        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Allowed operation (both owner of task and recipient of communication message are KVNRs of test card).
    //------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMin, {}, TaskAccessCode });

        // Pharmacy sends message to representative.
        const JWT jwtPharmacy{mJwtBuilder.makeJwtApotheke()};

        std::string jsonString =
            CommunicationJsonStringBuilder(Communication::MessageType::Reply)
                .setPrescriptionId(task.prescriptionId().toString())
                .setSender(ActorRole::Pharmacists, jwtPharmacy.stringForClaim(JWT::idNumberClaim).value())
                .setRecipient(ActorRole::Insurant, VerificationIdentityKvnrMax)
                .setPayload(ReplyMessage)
                .createJsonString();

        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of insured, recipient of communication message is KVNR of test card).
    //------------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });
        // Pharmacy sends message to representative.
        const JWT jwtPharmacy{mJwtBuilder.makeJwtApotheke()};

        std::string jsonString =
            CommunicationJsonStringBuilder(Communication::MessageType::Reply)
                .setPrescriptionId(task.prescriptionId().toString())
                .setSender(ActorRole::Pharmacists, jwtPharmacy.stringForClaim(JWT::idNumberClaim).value())
                .setRecipient(ActorRole::Insurant, VerificationIdentityKvnrMin)
                .setPayload(ReplyMessage)
                .createJsonString();

        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of test card, recipient of communication message is KVNR of insured).
    //------------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMax, {}, TaskAccessCode });

        // Pharmacy sends message to representative.
        const JWT jwtPharmacy{mJwtBuilder.makeJwtApotheke()};

        std::string jsonString =
            CommunicationJsonStringBuilder(Communication::MessageType::Reply)
                .setPrescriptionId(task.prescriptionId().toString())
                .setSender(ActorRole::Pharmacists, jwtPharmacy.stringForClaim(JWT::idNumberClaim).value())
                .setRecipient(ActorRole::Insurant, InsurantA)
                .setPayload(ReplyMessage)
                .createJsonString();

        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtPharmacy));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, DispReq_A20753_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    // Create a client
    auto client = createClient();

    A_20753.test("Exclusion of representative communication on or by means of verification identity");
    //================================================================================================

    // The e-prescription service MUST refuse any access to e-prescriptions using AccessCode (representative access)
    // with the http status code 400 if the access results in a combination of KVNR of the eGK test card and
    // KVNR of the insured:
    // - Insured persons are not allowed to access test prescriptions.
    // - With the eGK test card it is not allowed to access prescriptions from insured persons.

    // Allowed operation (both owner of task and sender of communication message are KVNRs of insurants).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Allowed operation (both owner of task and sender of communication message are KVNRs of test card).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMin, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMax) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of insured, sender of communication message is KVNR of test card).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMin) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of test card, sender of communication message is KVNR of insured).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMax, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(DispReqMessage).createJsonString();

        // Representative sends message to pharmacy.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantA) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, Representative_A20753_ExclusionOfVerificationIdentity)//NOLINT(readability-function-cognitive-complexity)
{
    // Create a client
    auto client = createClient();

    A_20753.test("Exclusion of representative communication on or by means of verification identity");
    //================================================================================================

    // The e-prescription service MUST refuse any access to e-prescriptions using AccessCode (representative access)
    // with the http status code 400 if the access results in a combination of KVNR of the eGK test card and
    // KVNR of the insured:
    // - Insured persons are not allowed to access test prescriptions.
    // - With the eGK test card it is not allowed to access prescriptions from insured persons.

    // Allowed operation (both owner of task and sender of communication message are KVNRs of insurants).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setPayload(RepresentativeMessageByRepresentative).createJsonString();

        // Representative sends message to insurant.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantB) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Allowed operation (both owner of task and sender of communication message are KVNRs of test card).
    //---------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMin, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setPayload(RepresentativeMessageByRepresentative).createJsonString();

        // Representative sends message to insurant.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMax) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of insured, sender of communication message is KVNR of test card).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, InsurantA)
            .setPayload(RepresentativeMessageByRepresentative).createJsonString();

        // Representative sends message to insurant.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(VerificationIdentityKvnrMin) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }

    // Not allowed operation (task belongs to KNVR of test card, sender of communication message is KVNR of insured).
    //----------------------------------------------------------------------------------------------------------------
    {
        auto task = addTaskToDatabase({ Task::Status::ready, VerificationIdentityKvnrMax, {}, TaskAccessCode });

        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
            .setPrescriptionId(task.prescriptionId().toString())
            .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
            .setAccessCode(std::string(task.accessCode()))
            .setPayload(RepresentativeMessageByRepresentative).createJsonString();

        // Representative sends message to insurant.
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantA) };
        // Create the inner request
        ClientRequest request1(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request1, jwtInsurant));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, InfoReq_MissingAboutTag)
{
    auto task = addTaskToDatabase({ Task::Status::ready, InsurantA, {} });

    // Create a client
    auto client = createClient();

    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId(task.prescriptionId().toString())
        .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
        .setPayload(InfoReqMessage)
        .createJsonString(); // no "about" attribute set

    const JWT jwtInsurant{mJwtBuilder.makeJwtVersicherter(InsurantA)};
    // Create the inner request
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant, std::string(task.accessCode())), jsonString);

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest, ContentMimeType::fhirJsonUtf8));
}

TEST_F(CommunicationPostHandlerTest, ChargChangeReq)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ .status = Task::Status::completed, .kvnrPatient = InsurantG, .accessCode = TaskAccessCode,
                                    .prescriptionType = model::PrescriptionType::apothekenpflichtigeArzneimittelPkv });
    const auto* telematikId = "3-SMC-B-Testkarte-883110000120312";
    EXPECT_NO_FATAL_FAILURE(
        addChargeItemToDatabase({ .prescriptionId = task.prescriptionId(), .kvnrStr = task.kvnr().value().id(),
                                  .accessCode = task.accessCode(), .telematikId = telematikId,
                                  .healthCarePrescriptionUuid = task.healthCarePrescriptionUuid().value() }));

    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReq)
                                 .setPrescriptionId(task.prescriptionId().toString())
                                 .setRecipient(ActorRole::Pharmacists, telematikId)
                                 .setPayload("ChargeChangeReq").createJsonString();

    auto client = createClient();
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value().id()) };

    ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    std::string location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
    Uuid id(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    std::optional<Communication> communication;
    ASSERT_NO_THROW(communication.emplace(
        ResourceFactory<Communication>::fromJson(innerResponse.getBody(), *StaticData::getJsonValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationChargChangeReq)));

    // The communication id must have been added to the json body.
    ASSERT_TRUE(communication->id().has_value());
    ASSERT_TRUE(communication->id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communication->timeSent().has_value());
    ASSERT_NO_FATAL_FAILURE(communication->timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_FATAL_FAILURE(communication->sender());
    ASSERT_EQ(model::getIdentityString(*communication->sender()), InsurantG);

    {
        // Check that it works with basedOn containing a full Url:
        jsonString = String::replaceAll(jsonString, "ChargeItem/", "https://erp.lu2.erezepttest.net:443/ChargeItem/");
        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirJsonUtf8));
    }
}

TEST_F(CommunicationPostHandlerTest, ChargChangeReply)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ .status = Task::Status::completed, .kvnrPatient = InsurantG, .accessCode = TaskAccessCode,
                                    .prescriptionType = model::PrescriptionType::apothekenpflichtigeArzneimittelPkv });
    const auto* telematikId = "3-SMC-B-Testkarte-883110000120312";
    EXPECT_NO_FATAL_FAILURE(
        addChargeItemToDatabase({ .prescriptionId = task.prescriptionId(), .kvnrStr = task.kvnr().value().id(),
                                  .accessCode = task.accessCode(), .telematikId = telematikId,
                                  .healthCarePrescriptionUuid = task.healthCarePrescriptionUuid().value() }));

    std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReply)
                                 .setPrescriptionId(task.prescriptionId().toString())
                                 .setSender(ActorRole::Pharmacists, telematikId)
                                 .setRecipient(ActorRole::Insurant, task.kvnr().value().id())
                                 .setPayload("ChargeChangeReply")
                                 .createJsonString();

    auto client = createClient();
    const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(telematikId) };
    ClientRequest request(createCommunicationPostHeader("/Communication", jwtPharmacy), jsonString);

    auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
    auto innerResponse = verifyOuterResponse(outerResponse);
    EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::Created, ContentMimeType::fhirXmlUtf8));

    EXPECT_EQ(innerResponse.getHeader().hasHeader(Header::Location), true);

    // The communication id must have been set in the header field "Location"
    std::string location = innerResponse.getHeader().header(Header::Location).value();
    EXPECT_EQ(String::starts_with(location, std::string(structure_definition::communicationLocation)), true);
    std::string idString = location.substr(std::string(structure_definition::communicationLocation).length());
    Uuid id(idString);
    EXPECT_TRUE(id.isValidIheUuid());

    std::optional<Communication> communication;
    ASSERT_NO_THROW(communication.emplace(
        ResourceFactory<Communication>::fromXml(innerResponse.getBody(), *StaticData::getXmlValidator(), {})
            .getValidated(model::ProfileType::Gem_erxCommunicationChargChangeReply)));

    // The communication id must have been added to the json body.
    ASSERT_TRUE(communication->id().has_value());
    ASSERT_TRUE(communication->id()->isValidIheUuid());

    // The time sent must have been added to the json body.
    ASSERT_TRUE(communication->timeSent().has_value());
    ASSERT_NO_FATAL_FAILURE(communication->timeSent());

    // The sender must have been taken from the access token.
    ASSERT_NO_FATAL_FAILURE(communication->sender());
    ASSERT_EQ(model::getIdentityString(*communication->sender()), telematikId);
}

TEST_F(CommunicationPostHandlerTest, ChargChangeReqErr)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ .status = Task::Status::completed, .kvnrPatient = InsurantG, .accessCode = TaskAccessCode,
                                    .prescriptionType = model::PrescriptionType::apothekenpflichtigeArzneimittelPkv });

    auto client = createClient();
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value().id()) };

    {
        // wrong prescription ID type (not PKV)
        const auto prescriptionId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::direkteZuweisung, task.prescriptionId().toDatabaseId());
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReq)
                                     .setPrescriptionId(prescriptionId.toString())
                                     .setSender(ActorRole::Insurant, task.kvnr().value().id())
                                     .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                     .setPayload("ChargeChangeReq")
                                     .createJsonString();

        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        // referenced ChargeItem does not exist
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReq)
                                     .setPrescriptionId(task.prescriptionId().toString())
                                     .setSender(ActorRole::Insurant, task.kvnr().value().id())
                                     .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                     .setPayload("ChargeChangeReq")
                                     .createJsonString();

        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
}

TEST_F(CommunicationPostHandlerTest, ChargChangeReplyErr)//NOLINT(readability-function-cognitive-complexity)
{
    auto task = addTaskToDatabase({ .status = Task::Status::completed, .kvnrPatient = InsurantG, .accessCode = TaskAccessCode,
                                    .prescriptionType = model::PrescriptionType::apothekenpflichtigeArzneimittelPkv });

    auto client = createClient();
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(task.kvnr().value().id()) };

    {
        // wrong prescription ID type (not PKV)
        const auto prescriptionId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::direkteZuweisung, task.prescriptionId().toDatabaseId());
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReply)
                                     .setPrescriptionId(prescriptionId.toString())
                                     .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                     .setPayload("ChargeChangeReply")
                                     .createJsonString();

        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
    {
        // referenced ChargeItem does not exist
        std::string jsonString = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReply)
                                     .setPrescriptionId(task.prescriptionId().toString())
                                     .setRecipient(ActorRole::Pharmacists, mPharmacy.id())
                                     .setPayload("ChargeChangeReply")
                                     .createJsonString();

        ClientRequest request(createCommunicationPostHeader("/Communication", jwtInsurant), jsonString);
        auto outerResponse = client.send(encryptRequest(request, jwtInsurant));
        auto innerResponse = verifyOuterResponse(outerResponse);
        EXPECT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, HttpStatus::BadRequest));
    }
}
