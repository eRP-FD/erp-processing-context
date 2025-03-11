/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test_config.h"
#include "test/erp/model/CommunicationTest.hxx"
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
    std::string bodyRequest =
        CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
            .setPrescriptionId("160.123.456.789.123.58")
            .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
            .setRecipient(ActorRole::Pharmacists, "PharmacyA")
            .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.")
            .createJsonString();
    std::optional<Communication> communication1;
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const gsl::not_null repoView = fhirInstance.structureRepository(model::Timestamp::now())
        .match(backend, std::string{structure_definition::communicationInfoReq},
               ResourceTemplates::Versions::GEM_ERP_current());
    ASSERT_NO_THROW(communication1 = model::ResourceFactory<Communication>::fromJson(
                                         bodyRequest, *StaticData::getJsonValidator(), {})
                                         .getValidated(ProfileType::Gem_erxCommunicationInfoReq, repoView));
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
              std::string{model::resource::structure_definition::communicationInfoReq} + '|' +
                  to_string(ResourceTemplates::Versions::GEM_ERP_current()));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer),
              naming_system::telematicID);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyA");

    EXPECT_EQ(communication.messageType(), Communication::MessageType::InfoReq);
    EXPECT_ANY_THROW(communication.profileType()); // InfoReq currently not allowed
    EXPECT_EQ(communication.messageTypeAsString(), "InfoReq");

    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationInfoReq);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(model::getIdentityString(communication.recipient()), "PharmacyA");
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateReplyFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest =
        CommunicationJsonStringBuilder(Communication::MessageType::Reply)
            .setPrescriptionId("160.123.456.789.123.58")
            .setSender(ActorRole::Pharmacists, "SMC-B-0815-4711")
            .setRecipient(ActorRole::Insurant, InsurantA)
            .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.")
            .createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 =
                        ResourceFactory<Communication>::fromJson(bodyRequest, *StaticData::getJsonValidator(), {})
                            .getValidated(ProfileType::Gem_erxCommunicationReply));
    auto& communication = *communication1;

    ASSERT_NE(Communication::resourceTypePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::idPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::metaProfile0Pointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::basedOn0ReferencePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::senderIdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::senderIdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierSystemPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_NE(Communication::recipient0IdentifierValuePointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::sentPointer.Get(communication.jsonDocument()), nullptr);
    ASSERT_EQ(Communication::receivedPointer.Get(communication.jsonDocument()), nullptr);

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::metaProfile0Pointer),
              std::string{model::resource::structure_definition::communicationReply} + '|' +
                  to_string(ResourceTemplates::Versions::GEM_ERP_current()));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58");
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer),
              naming_system::gkvKvid10);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), InsurantA);

    EXPECT_EQ(communication.messageType(), Communication::MessageType::Reply);
    EXPECT_EQ(communication.profileType(), model::ProfileType::Gem_erxCommunicationReply);
    EXPECT_EQ(communication.messageTypeAsString(), "Reply");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationReply);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_TRUE(communication.sender().has_value());
    EXPECT_EQ(model::getIdentityString(communication.recipient()), InsurantA);
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateDispReqFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    auto profileVersion = ResourceTemplates::Versions::GEM_ERP_current();
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq, profileVersion)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Pharmacists, "PharmacyA")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 =
                        ResourceFactory<Communication>::fromJson(bodyRequest, *StaticData::getJsonValidator(), {})
                            .getValidated(ProfileType::Gem_erxCommunicationDispReq));
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
              std::string{model::resource::structure_definition::communicationDispReq} + '|' +
                  to_string(ResourceTemplates::Versions::GEM_ERP_current()));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer),
              naming_system::telematicID);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyA");

    EXPECT_EQ(communication.messageType(), Communication::MessageType::DispReq);
    EXPECT_EQ(communication.profileType(), model::ProfileType::Gem_erxCommunicationDispReq);
    EXPECT_EQ(communication.messageTypeAsString(), "DispReq");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationDispReq);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_TRUE(communication.accessCode().has_value());
    EXPECT_EQ(communication.accessCode(), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(model::getIdentityString(communication.recipient()), "PharmacyA");
    EXPECT_EQ(communication.timeSent().has_value(), false);
    EXPECT_EQ(communication.timeReceived().has_value(), false);

    EXPECT_EQ(communication.serializeToJsonString(), canonicalJson(bodyRequest));
    EXPECT_NO_THROW((void)communication.serializeToXmlString());
}

TEST_F(CommunicationTest, CreateRepresentativeFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    auto profileVersion = ResourceTemplates::Versions::GEM_ERP_current();
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative, profileVersion)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setRecipient(ActorRole::Insurant, InsurantA)
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();

    std::optional<Communication> communication1;
    ASSERT_NO_THROW(communication1 =
                        ResourceFactory<Communication>::fromJson(bodyRequest, *StaticData::getJsonValidator(), {})
                            .getValidated(ProfileType::Gem_erxCommunicationRepresentative));
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
              std::string{model::resource::structure_definition::communicationRepresentative} + '|' +
                  to_string(ResourceTemplates::Versions::GEM_ERP_current()));
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::basedOn0ReferencePointer), "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");

    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer),
              naming_system::gkvKvid10);
    EXPECT_EQ(communication.jsonDocument().getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), InsurantA);

    EXPECT_EQ(communication.messageType(), Communication::MessageType::Representative);
    EXPECT_EQ(communication.profileType(), model::ProfileType::Gem_erxCommunicationRepresentative);
    EXPECT_EQ(communication.messageTypeAsString(), "Representative");
    EXPECT_EQ(communication.messageTypeAsProfileUrl(), structure_definition::communicationRepresentative);

    EXPECT_EQ(communication.id().has_value(), false);
    EXPECT_EQ(communication.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_TRUE(communication.accessCode().has_value());
    EXPECT_EQ(communication.accessCode(), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
    EXPECT_EQ(communication.sender().has_value(), false);
    EXPECT_EQ(model::getIdentityString(communication.recipient()), InsurantA);
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
    communication.setSender(model::Kvnr{"X234567891"});

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()),
              naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), "X234567891");

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), "X234567891");

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender(model::Kvnr{"X098765439"});
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), "X098765439");
}

TEST_F(CommunicationTest, InfoReqSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::InfoReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de")
        .setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient(model::TelematikId{"PharmacyA"});

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()),
              naming_system::telematicID);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), "PharmacyA");

    ASSERT_EQ(model::getIdentityString(communication.recipient()), "PharmacyA");

    // This call updates the already existing member "/recipient/0/identifier/value".
    communication.setRecipient(model::TelematikId{"PharmacyC"});
    ASSERT_EQ(model::getIdentityString(communication.recipient()), "PharmacyC");
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
    communication.setSender(model::TelematikId{"123456789"});

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()),
              naming_system::telematicID);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), "123456789");

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), "123456789");

    // This call updates the already existing member "/sender/identifier/value".
    communication.setSender(model::TelematikId{"987654321"});
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), "987654321");
}

TEST_F(CommunicationTest, ReplySetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Reply)
        .setPrescriptionId("160.123.456.789.123.58")
        .setPayload("Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);

    model::Kvnr kvnrInsurantC{InsurantC};

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    communication.setRecipient(kvnrInsurantC);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()),
              naming_system::gkvKvid10);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), kvnrInsurantC.id());

    ASSERT_EQ(model::getIdentityString(communication.recipient()), InsurantC);

    // This call updates the already existing member "/recipient/0/identifier/value".
    const auto kvnrNew = model::Kvnr{InsurantX};
    communication.setRecipient(kvnrNew);
    ASSERT_EQ(model::getIdentityString(communication.recipient()), InsurantX);
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

    const model::Kvnr kvnrA{InsurantA};
    // This call adds the members "/sender/identifier/system" and "/sender/identifier/value".
    communication.setSender(kvnrA);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()),
              naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), InsurantA);

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), InsurantA);

    // This call updates the already existing member "/sender/identifier/value".
    const model::Kvnr kvnrB{InsurantB};
    communication.setSender(kvnrB);
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), kvnrB);
}

TEST_F(CommunicationTest, DispReqSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::DispReq)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setPayload("Bitte schicken Sie einen Boten.").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    const model::TelematikId pharmacyA{"PharmacyA"};
    communication.setRecipient(pharmacyA);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()),
              naming_system::telematicID);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), pharmacyA.id());

    ASSERT_EQ(model::getIdentityString(communication.recipient()), pharmacyA);

    // This call updates the already existing member "/recipient/0/identifier/value".
    const model::TelematikId pharmacyX{"PharmacyX"};
    communication.setRecipient(pharmacyX);
    ASSERT_EQ(model::getIdentityString(communication.recipient()), pharmacyX);
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
    const model::Kvnr kvnrB{InsurantB};
    communication.setSender(kvnrB);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::senderIdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::senderIdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::senderIdentifierSystemPointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierSystemPointer.Get(document)->GetString()),
              naming_system::gkvKvid10);
    EXPECT_EQ(Communication::senderIdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::senderIdentifierValuePointer.Get(document)->GetString()), InsurantB);

    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), kvnrB);

    // This call updates the already existing member "/sender/identifier/value".
    const model::Kvnr kvnrC{InsurantC};
    communication.setSender(kvnrC);
    EXPECT_EQ(communication.sender().has_value(), true);
    ASSERT_EQ(model::getIdentityString(*communication.sender()), kvnrC);
}

TEST_F(CommunicationTest, RepresentativeSetRecipient)//NOLINT(readability-function-cognitive-complexity)
{
    auto profileVersion = ResourceTemplates::Versions::GEM_ERP_current();
    std::string bodyRequest = CommunicationJsonStringBuilder(Communication::MessageType::Representative, profileVersion)
        .setPrescriptionId("160.123.456.789.123.58")
        .setAccessCode("777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea")
        .setPayload("Kannst Du das Medikament fuer mich holen?").createJsonString();
    Communication communication = Communication::fromJsonNoValidation(bodyRequest);

    // This call adds the members "/recipient/0/identifier/system" and "/recipient/0/identifier/value".
    const model::Kvnr kvnrB{InsurantB};
    communication.setRecipient(kvnrB);

    std::string jsonString = communication.serializeToJsonString();

    rapidjson::Document document;
    document.Parse(jsonString);
    EXPECT_EQ(document.HasParseError(), false);

    EXPECT_NE(Communication::recipient0IdentifierSystemPointer.Get(document), nullptr);
    EXPECT_NE(Communication::recipient0IdentifierValuePointer.Get(document), nullptr);

    EXPECT_EQ(Communication::recipient0IdentifierSystemPointer.Get(document)->IsString(), true);

    EXPECT_EQ(std::string(Communication::recipient0IdentifierSystemPointer.Get(document)->GetString()),
              naming_system::gkvKvid10);
    EXPECT_EQ(Communication::recipient0IdentifierValuePointer.Get(document)->IsString(), true);
    EXPECT_EQ(std::string(Communication::recipient0IdentifierValuePointer.Get(document)->GetString()), InsurantB);

    ASSERT_EQ(model::getIdentityString(communication.recipient()), kvnrB);

    // This call updates the already existing member "/recipient/0/identifier/value".
    const model::Kvnr kvnrX{InsurantX};
    communication.setRecipient(kvnrX);
    ASSERT_EQ(model::getIdentityString(communication.recipient()), kvnrX);
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
                           .setRecipient(ActorRole::Pharmacists, "PharmacyX")
                           .setPayload("Some change request payload")
                           .createJsonString();

    const auto chargChangeReqComm =
        ResourceFactory<Communication>::fromJson(std::move(body), *StaticData::getJsonValidator(), {})
            .getValidated(ProfileType::Gem_erxCommunicationChargChangeReq);

    EXPECT_EQ(chargChangeReqComm.messageType(), Communication::MessageType::ChargChangeReq);
    EXPECT_EQ(chargChangeReqComm.profileType(), model::ProfileType::Gem_erxCommunicationChargChangeReq);
    EXPECT_EQ(chargChangeReqComm.messageTypeAsString(), "ChargChangeReq");
    EXPECT_EQ(chargChangeReqComm.messageTypeAsProfileUrl(), structure_definition::communicationChargChangeReq);
    EXPECT_FALSE(chargChangeReqComm.id().has_value());
    EXPECT_EQ(chargChangeReqComm.prescriptionId().toString(), "160.123.456.789.123.58");
    EXPECT_FALSE(chargChangeReqComm.sender().has_value());
    EXPECT_EQ(model::getIdentityString(chargChangeReqComm.recipient()), "PharmacyX");
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
    EXPECT_EQ(json.getStringValueFromPointer(Communication::basedOn0ReferencePointer), "ChargeItem/160.123.456.789.123.58");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::telematicID);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), "PharmacyX");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::metaProfile0Pointer),
              std::string{model::resource::structure_definition::communicationChargChangeReq} + '|' +
                  to_string(ResourceTemplates::Versions::GEM_ERPCHRG_current()));
}

TEST_F(CommunicationTest, CreateChargChangeReplyFromJson)
{
    std::string body = CommunicationJsonStringBuilder(Communication::MessageType::ChargChangeReply)
    .setPrescriptionId("160.123.456.789.123.58")
    .setSender(ActorRole::Pharmacists, "PharmacyX")
    .setRecipient(ActorRole::PkvInsurant, InsurantA)
    .setPayload("Some change request payload")
    .createJsonString();

    const auto chargChangeReplyComm =
    ResourceFactory<Communication>::fromJson(std::move(body), *StaticData::getJsonValidator(), {})
    .getValidated(ProfileType::Gem_erxCommunicationChargChangeReply);

    EXPECT_EQ(chargChangeReplyComm.messageType(), Communication::MessageType::ChargChangeReply);
    EXPECT_EQ(chargChangeReplyComm.profileType(), model::ProfileType::Gem_erxCommunicationChargChangeReply);
    EXPECT_EQ(chargChangeReplyComm.messageTypeAsString(), "ChargChangeReply");
    EXPECT_EQ(chargChangeReplyComm.messageTypeAsProfileUrl(), structure_definition::communicationChargChangeReply);
    EXPECT_FALSE(chargChangeReplyComm.id().has_value());
    EXPECT_EQ(chargChangeReplyComm.prescriptionId().toString(), "160.123.456.789.123.58");
    ASSERT_TRUE(chargChangeReplyComm.sender().has_value());
    EXPECT_EQ(model::getIdentityString(chargChangeReplyComm.sender().value()), "PharmacyX");
    EXPECT_EQ(model::getIdentityString(chargChangeReplyComm.recipient()), InsurantA);
    EXPECT_FALSE(chargChangeReplyComm.timeSent().has_value());
    EXPECT_FALSE(chargChangeReplyComm.timeReceived().has_value());

    const auto& json = chargChangeReplyComm.jsonDocument();
    EXPECT_NE(Communication::resourceTypePointer.Get(json), nullptr);
    EXPECT_EQ(Communication::idPointer.Get(json), nullptr);
    EXPECT_NE(Communication::metaProfile0Pointer.Get(json), nullptr);
    EXPECT_NE(Communication::basedOn0ReferencePointer.Get(json), nullptr);
    EXPECT_EQ(Communication::sentPointer.Get(json), nullptr);
    EXPECT_EQ(Communication::receivedPointer.Get(json), nullptr);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::resourceTypePointer), "Communication");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::basedOn0ReferencePointer), "ChargeItem/160.123.456.789.123.58");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::senderIdentifierSystemPointer), naming_system::telematicID);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::senderIdentifierValuePointer), "PharmacyX");
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierSystemPointer), naming_system::pkvKvid10);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::recipient0IdentifierValuePointer), InsurantA);
    EXPECT_EQ(json.getStringValueFromPointer(Communication::metaProfile0Pointer),
              std::string{model::resource::structure_definition::communicationChargChangeReply} + '|' +
    to_string(ResourceTemplates::Versions::GEM_ERPCHRG_current()));
}

namespace
{

enum ExpectedOutcome
{
    success,
    failure,
};

void testVerifyPayload(Communication::MessageType messageType, std::string_view payload,
                       ExpectedOutcome expectedOutcome, std::optional<std::string_view> expectedMessage = std::nullopt,
                       bool withDiagnostics = true)
{
    auto builder = CommunicationJsonStringBuilder(messageType);
    builder.setPayload(std::string{payload})
        .setPrescriptionId(
            PrescriptionId::fromDatabaseId(PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4711).toString());
    switch (messageType)
    {
        using enum Communication::MessageType;
        case InfoReq:
        case DispReq:
            builder.setAbout("#5e164c7d-8dc4-4e46-9b25-163f05bd203f");
            [[fallthrough]];
        case ChargChangeReq:
            builder.setSender(ActorRole::Insurant, "X555000111")
                .setRecipient(ActorRole::Pharmacists, "SMC-B-0815-4711");
            break;
        case Representative:
            builder.setSender(ActorRole::Insurant, "X666000111").setRecipient(ActorRole::Insurant, "X555000111");
            break;
        case Reply:
        case ChargChangeReply:
            builder.setSender(ActorRole::Pharmacists, "SMC-B-0815-4711")
                .setRecipient(ActorRole::Insurant, "X555000111");
    }

    std::string body = builder.createJsonString();
    auto jsonValidator = StaticData::getJsonValidator();
    const auto comm = ResourceFactory<Communication>::fromJson(
                          std::move(body), *jsonValidator, {.genericValidationMode = GenericValidationMode::disable})
                          .getValidated(Communication::messageTypeToProfileType(messageType));
    try
    {
        comm.verifyPayload(*jsonValidator);
    }
    catch (const ErpException& e)
    {
        ASSERT_EQ(expectedOutcome, ExpectedOutcome::failure);
        if (expectedMessage.has_value())
        {
            ASSERT_TRUE(String::starts_with(std::string_view{e.what()}, *expectedMessage));
            ASSERT_EQ(e.diagnostics().has_value(), withDiagnostics) << body;
        }
        return;
    }
    catch (const std::exception& e)
    {
        ASSERT_EQ(expectedOutcome, ExpectedOutcome::failure);
        if (expectedMessage.has_value())
        {
            ASSERT_TRUE(String::starts_with(std::string_view{e.what()}, *expectedMessage));
        }
        return;
    }

    ASSERT_EQ(expectedOutcome, ExpectedOutcome::success);
}

}

TEST_F(CommunicationTest, verifyPayloadDispReq)
{
    const auto msgType = Communication::MessageType::DispReq;
    A_23878.test("Validierung DispReq payload");

    // simple success cases
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"shipment","name": "Hans Peter Willich","address":["Hausnummer 1","3.OG","12345 Berlin"],"hint":"bla","phone":"004916094858168"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"delivery","name": "Hans Peter Willich","address":["Hausnummer 1","3.OG","12345 Berlin"],"hint":"bla","phone":"004916094858168"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }

    // failures
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"invalid"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({"version":2,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"(not-a-json)";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid JSON in payload.contentString", false));
    }
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise","extraElement":"1"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    // failures with too large field lengths
    {
        std::string longName(101, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","name":")" + longName + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","name":")" + longName.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longHint(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","hint":")" + longHint + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","hint":")" + longHint.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longPhone(33, '0');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","phone":")" + longPhone + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","phone":")" + longPhone.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longAddress(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","address":[")" + longAddress + "\",\"" +
                              longAddress + R"("]})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","address":[")" + longAddress.substr(1) + "\",\"" +
                  longAddress.substr(1) + R"("]})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
}

TEST_F(CommunicationTest, verifyPayloadReply)
{
    A_23879.test("Validierung DispReq payload");
    const auto msgType = Communication::MessageType::Reply;

    // simple success cases
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"onPremise","info_text":"Hallo","url":"https://gematik.de","pickUpCodeHR":"12345678","pickUpCodeDMC":"123459789"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"delivery","info_text":"Hallo der Herr","url":"https://gematik.de"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"shipment","info_text":"Hallo der Herr","url":"https://www.example.com/path/to/file.txt?userid=1001&pages=3&results=full#page1"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    // failures
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"invalid"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({"version":2,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"(not-a-json)";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid JSON in payload.contentString"));
    }
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise","extraElement":"1"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    // failures with too large field lengths
    {
        std::string longInfo(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","info_text":")" + longInfo + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","info_text":")" + longInfo.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string url{"https://gematik.de/"};
        url.append(std::string(501 - url.size(), 'A'));
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string pickUpCodeHR(9, 'A');
        std::string payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeHR":")" + pickUpCodeHR + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeHR":")" + pickUpCodeHR.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string pickUpCodeDMC(129, 'A');
        std::string payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeDMC":")" + pickUpCodeDMC + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeDMC":")" + pickUpCodeDMC.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    // requirement to have supplyOptionsType = "onPremise" when pickUpCodeDMC or pickUpCodeHR are set
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"shipment","pickUpCodeDMC":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"delivery","pickUpCodeDMC":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"shipment","pickUpCodeHR":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"delivery","pickUpCodeHR":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string url{"something"};
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid payload: URL not valid."));
    }
}


TEST_F(CommunicationTest, verifyPayloadNoJson)
{
    using enum model::Communication::MessageType;
    std::string_view payload = "not-a-json";
    std::vector<model::Communication::MessageType> messageTypes;
    // 2023 profiles must all, without DispReq & Reply
    messageTypes = {InfoReq, Representative, ChargChangeReply, ChargChangeReq};

    for (const auto& msgType : messageTypes)
    {
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
}
