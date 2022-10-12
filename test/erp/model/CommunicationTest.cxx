/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/erp/model/CommunicationTest.hxx"
#include "erp/model/ResourceNames.hxx"
#include "test_config.h"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/StaticData.hxx"

#include <rapidjson/stringbuffer.h>

using namespace model;
using namespace model::resource;


CommunicationTest::CommunicationTest() :
    mDataPath{ std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest" }
{
}

TEST_F(CommunicationTest, CreateInfoReqFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 = Communication::fromJson(bodyRequest, *StaticData::getJsonValidator(),
                                                             *StaticData::getXmlValidator(),
                                                             *StaticData::getInCodeValidator(),
                                                             SchemaType::Gem_erxCommunicationInfoReq));
    auto& communication = *communication1;

    ASSERT_NE(Communication::resourceTypePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::idPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::metaProfile0Pointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::basedOn0ReferencePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::sentPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::receivedPointer.Get(communication.jsonDocument()), nullptr);

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::metaProfile0Pointer),
              ::model::ResourceVersion::versionizeProfile(structure_definition::communicationInfoReq));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::telematicID);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyA");

    EXPECT_EQ(communication.messageType(), Communication::MessageType::InfoReq);
    EXPECT_EQ(communication.messageTypeAsString(), "InfoReq");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationInfoReq);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(communication.recipient().has_value(), true);
    EXPECT_EQ(communication.recipient(), "PharmacyA");
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateReplyFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 = Communication::fromJson(bodyRequest, *StaticData::getJsonValidator(),
                                                             *StaticData::getXmlValidator(),
                                                             *StaticData::getInCodeValidator(),
                                                             SchemaType::Gem_erxCommunicationReply, std::nullopt));
    auto& communication = *communication1;

    ASSERT_NE(Communication::resourceTypePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::idPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::metaProfile0Pointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::basedOn0ReferencePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::sentPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::receivedPointer.Get(communication.jsonDocument()), nullptr);

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::metaProfile0Pointer),
              ::model::ResourceVersion::versionizeProfile(structure_definition::communicationReply));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::gkvKvid10);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), InsurantA);

    EXPECT_EQ(communication.messageType(), Communication::MessageType::Reply);
    EXPECT_EQ(communication.messageTypeAsString(), "Reply");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationReply);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(communication.recipient().has_value(), true);
    EXPECT_EQ(communication.recipient(), InsurantA);
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateDispReqFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 = Communication::fromJson(bodyRequest, *StaticData::getJsonValidator(),
                                                             *StaticData::getXmlValidator(),
                                                             *StaticData::getInCodeValidator(),
                                                             SchemaType::Gem_erxCommunicationDispReq));
    auto& communication = *communication1;

    ASSERT_NE(Communication::resourceTypePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::idPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::metaProfile0Pointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::basedOn0ReferencePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::sentPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::receivedPointer.Get(communication.jsonDocument()), nullptr);

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::metaProfile0Pointer),
              ::model::ResourceVersion::versionizeProfile(structure_definition::communicationDispReq));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::telematicID);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyA");

    EXPECT_EQ(communication.messageType(), Communication::MessageType::DispReq);
    EXPECT_EQ(communication.messageTypeAsString(), "DispReq");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationDispReq);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_TRUE(communication.accessCode().has_value());
    EXPECT_EQ(communication.accessCode(), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(communication.recipient().has_value(), true);
    EXPECT_EQ(communication.recipient(), "PharmacyA");
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateRepresentativeFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 = Communication::fromJson(bodyRequest, *StaticData::getJsonValidator(),
                                                             *StaticData::getXmlValidator(),
                                                             *StaticData::getInCodeValidator(),
                                                             SchemaType::Gem_erxCommunicationRepresentative));
    auto& communication = *communication1;

    ASSERT_NE(Communication::resourceTypePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::idPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::metaProfile0Pointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::basedOn0ReferencePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::senderIdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::sentPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::receivedPointer.Get(communication.jsonDocument()), nullptr);

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::metaProfile0Pointer),
              ::model::ResourceVersion::versionizeProfile(structure_definition::communicationRepresentative));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::gkvKvid10);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), InsurantA);

    EXPECT_EQ(communication.messageType(), Communication::MessageType::Representative);
    EXPECT_EQ(communication.messageTypeAsString(), "Representative");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationRepresentative);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_TRUE(communication.accessCode().has_value());
    EXPECT_EQ(communication.accessCode(), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(communication.recipient().has_value(), true);
    EXPECT_EQ(communication.recipient(), InsurantA);
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, InfoReqSetId)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.id().has_value(), false);

    // This call adds the member "id" below the root.
    Uuid uuid;
    communication.setId(uuid);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::idPointer.Get(document), nullptr);

    EXPECT_EQ(Communication::idPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::idPointer.Get(document)->GetString()), uuid.toString());

    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid);

    // This call updates the value of the already existing member "id".
    Uuid uuid2;
    communication.setId(uuid2);
    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid2);
}

TEST_F(CommunicationTest, InfoReqSetSender)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.sender().has_value(), false);

    // This call adds the members "/sender/identifier/system" and "/sender/identifier/value".
    communication.setSender("X234567890");

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()), naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), "X234567890");

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), "X234567890");

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender("X098765432");
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), "X098765432");
}

TEST_F(CommunicationTest, InfoReqSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.recipient().has_value(), false);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient("PharmacyA");

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()), naming_system::telematicID);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), "PharmacyA");

    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), "PharmacyA");

    // This call updates the already existing member "/recipient/0/identifier/value".
    communication.setRecipient("PharmacyC");
    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), "PharmacyC");
}

TEST_F(CommunicationTest, InfoReqSetTimeSent)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeSent().has_value(), false);

    // This call adds the member "sent".
    communication.setTimeSent(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::sentPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::sentPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());

    // This call updates the already existing member "sent".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeSent(timestamp);
    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());
}

TEST_F(CommunicationTest, InfoReqSetTimeReceived)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    // This call adds the member "received".
    communication.setTimeReceived(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::receivedPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::receivedPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(communication.timeReceived(), timestamp);

    // This call updates the already existing member "received".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeReceived(timestamp);
    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeReceived());
}

TEST_F(CommunicationTest, ReplySetId)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.id().has_value(), false);

    // This call adds the member "id" below the root.
    Uuid uuid;
    communication.setId(uuid);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::idPointer.Get(document), nullptr);

    EXPECT_EQ(Communication::idPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::idPointer.Get(document)->GetString()), uuid.toString());

    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid);

    // This call updates the value of the already existing member "id".
    Uuid uuid2;
    communication.setId(uuid2);
    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid2);
}

TEST_F(CommunicationTest, ReplySetSender)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.sender().has_value(), false);

    // This call adds the members "/sender/identifier/system" and "/sender/identifier/value".
    communication.setSender("123456789");

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()), naming_system::telematicID);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), "123456789");

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), "123456789");

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender("987654321");
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), "987654321");
}

TEST_F(CommunicationTest, ReplySetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.recipient().has_value(), false);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient(InsurantC);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()), naming_system::gkvKvid10);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), InsurantC);

    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), InsurantC);

    // This call updates the already existing member "/recipient/0/identifier/value".
    communication.setRecipient(InsurantX);
    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), InsurantX);
}

TEST_F(CommunicationTest, ReplySetTimeSent)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeSent().has_value(), false);

    // This call adds the member "sent".
    communication.setTimeSent(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::sentPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::sentPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());

    // This call updates the already existing member "sent".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeSent(timestamp);
    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());
}

TEST_F(CommunicationTest, ReplySetTimeReceived)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    // This call adds the member "received".
    communication.setTimeReceived(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::receivedPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::receivedPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(communication.timeReceived(), timestamp);

    // This call updates the already existing member "received".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeReceived(timestamp);
    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeReceived());
}

TEST_F(CommunicationTest, DispReqSetId)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.id().has_value(), false);

    // This call adds the member "id" below the root.
    Uuid uuid;
    communication.setId(uuid);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::idPointer.Get(document), nullptr);

    EXPECT_EQ(Communication::idPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::idPointer.Get(document)->GetString()), uuid.toString());

    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid);

    // This call updates the value of the already existing member "id".
    Uuid uuid2;
    communication.setId(uuid2);
    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid2);
}

TEST_F(CommunicationTest, DispReqSetSender)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.sender().has_value(), false);

    // This call adds the members "/sender/identifier/system" and "/sender/identifier/value".
    communication.setSender(InsurantA);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()), naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), InsurantA);

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), InsurantA);

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender(InsurantB);
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), InsurantB);
}

TEST_F(CommunicationTest, DispReqSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.recipient().has_value(), false);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient("PharmacyA");

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()), naming_system::telematicID);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), "PharmacyA");

    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), "PharmacyA");

    // This call updates the already existing member "/recipient/0/identifier/value".
    communication.setRecipient("PharmacyX");
    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), "PharmacyX");
}

TEST_F(CommunicationTest, DispReqSetTimeSent)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeSent().has_value(), false);

    // This call adds the member "sent".
    communication.setTimeSent(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::sentPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::sentPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());

    // This call updates the already existing member "sent".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeSent(timestamp);
    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());
}

TEST_F(CommunicationTest, DispReqSetTimeReceived)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    // This call adds the member "received".
    communication.setTimeReceived(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::receivedPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::receivedPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(communication.timeReceived(), timestamp);

    // This call updates the already existing member "received".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeReceived(timestamp);
    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeReceived());
}

TEST_F(CommunicationTest, RepresentativeSetId)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.id().has_value(), false);

    // This call adds the member "id" below the root.
    Uuid uuid;
    communication.setId(uuid);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::idPointer.Get(document), nullptr);

    EXPECT_EQ(Communication::idPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::idPointer.Get(document)->GetString()), uuid.toString());

    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid);

    // This call updates the value of the already existing member "id".
    Uuid uuid2;
    communication.setId(uuid2);
    EXPECT_EQ(communication.id().has_value(), true);
    ASSERT_EQ(communication.id(), uuid2);
}

TEST_F(CommunicationTest, RepresentativeSetSender)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.sender().has_value(), false);

    // This call adds the members "/sender/identifier/system" and "/sender/identifier/value".
    communication.setSender(InsurantB);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()), naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), InsurantB);

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), InsurantB);

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender(InsurantC);
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(communication.sender(), InsurantC);
}

TEST_F(CommunicationTest, RepresentativeSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    EXPECT_EQ(communication.recipient().has_value(), false);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient(InsurantB);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()), naming_system::gkvKvid10);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), InsurantB);

    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), InsurantB);

    // This call updates the already existing member "/recipient/0/identifier/value".
    communication.setRecipient(InsurantX);
    EXPECT_EQ(communication.recipient().has_value(), true);
    ASSERT_EQ(communication.recipient(), InsurantX);
}

TEST_F(CommunicationTest, RepresentativeSetTimeSent)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeSent().has_value(), false);

    // This call adds the member "sent".
    communication.setTimeSent(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::sentPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::sentPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());

    // This call updates the already existing member "sent".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeSent(timestamp);
    EXPECT_EQ(communication.timeSent().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeSent());
}

TEST_F(CommunicationTest, RepresentativeSetTimeReceived)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);
    Timestamp timestamp = Timestamp::now();
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    // This call adds the member "received".
    communication.setTimeReceived(timestamp);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::receivedPointer.Get(document), nullptr);
    EXPECT_EQ(std::string(Communication::receivedPointer.Get(document)->GetString()), timestamp.toXsDateTime());

    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(communication.timeReceived(), timestamp);

    // This call updates the already existing member "received".
    timestamp = Timestamp::fromXsDateTime("2021-01-28T11:09:02.156+00:00");
    communication.setTimeReceived(timestamp);
    EXPECT_EQ(communication.timeReceived().has_value(), true);
    ASSERT_EQ(timestamp, communication.timeReceived());
}

TEST_F(CommunicationTest, CreateChargChangeReqFromJson)
{
    std::string body = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReq)
                           .setPrescriptionId("160.123.456.789.123.58")
                           .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
                           .setRecipient(ActorRole::Pharmacists, "PharmacyX")
                           .setPayload("Some change request payload")
                           .createJsonString();

    const auto chargChangeReqComm = Communication::fromJson(
        std::move(body),
        *StaticData::getJsonValidator(),
        *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(),
        SchemaType::fhir, std::nullopt);
    
    EXPECT_EQ(chargChangeReqComm.messageType(), Communication::MessageType::ChargChangeReq);
    EXPECT_EQ(chargChangeReqComm.messageTypeAsString(), "ChargChangeReq");
    EXPECT_EQ(chargChangeReqComm.messageTypeAsProfileUrl(), structure_definition::communicationChargChangeReq);
    EXPECT_FALSE(chargChangeReqComm.id().has_value());
    EXPECT_EQ(chargChangeReqComm.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_FALSE(chargChangeReqComm.sender().has_value());
    EXPECT_TRUE(chargChangeReqComm.recipient().has_value());
    EXPECT_EQ(chargChangeReqComm.recipient(), "PharmacyX");
    EXPECT_FALSE(chargChangeReqComm.timeSent().has_value());
    EXPECT_FALSE(chargChangeReqComm.timeReceived().has_value());

    const auto& json = chargChangeReqComm.jsonDocument();
    EXPECT_NE(Communication::resourceTypePointer.Get(json), nullptr);
    EXPECT_EQ(Communication::idPointer.Get(json), nullptr);
    EXPECT_NE(Communication::metaProfile0Pointer.Get(json), nullptr);
    EXPECT_NE(Communication::basedOn0ReferencePointer.Get(json), nullptr);
    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(json), nullptr);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(json), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(json), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(json), nullptr);
    EXPECT_EQ(Communication::sentPointer.Get(json), nullptr);
    EXPECT_EQ(Communication::receivedPointer.Get(json), nullptr);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::telematicID);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyX");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::metaProfile0Pointer),
              model::ResourceVersion::versionizeProfile(structure_definition::communicationChargChangeReq));
}
