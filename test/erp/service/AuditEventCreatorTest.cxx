/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/AuditEventCreator.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/model/ResourceNames.hxx"
#include "tools/jwt/JwtBuilder.hxx"

#include "tools/ResourceManager.hxx"

#include <gtest/gtest.h>


TEST(AuditEventCreatorTest, createRepresentative)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_Task_id_representative;
    const model::AuditEvent::Action action = model::AuditEvent::Action::read;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const std::string_view kvnr = "X424242424";
    const std::string_view agentName = "Max Mustermann";
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData(agentName, kvnr), action, agentType, insurantKvnr, deviceId, prescriptionId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)));
    const auto* language = "de";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
        " hat ein E-Rezept " + prescriptionId.toString() + " heruntergeladen.</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::read);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::gkvKvid10);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue, kvnr);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
    EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityWhatReference(), "Task/" + prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}


TEST(AuditEventCreatorTest, createPatient)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Task_abort_insurant;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }), action, agentType, insurantKvnr, deviceId, prescriptionId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(insurantKvnr)));
    const auto* language = "en";
    const auto agentName = jwt->stringForClaim(JWT::givenNameClaim).value() + " " +
                           jwt->stringForClaim(JWT::familyNameClaim).value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " deleted an e-prescription " + prescriptionId.toString() + ".</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::del);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem, model::resource::naming_system::gkvKvid10);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), insurantKvnr);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
    EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityWhatReference(), "Task/"+prescriptionId.toString()+ "/$abort");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST(AuditEventCreatorTest, createGetMultipleResources)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_MedicationDispense;
    const model::AuditEvent::Action action = model::AuditEvent::Action::read;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }), action, agentType, insurantKvnr, deviceId, {} /*no prescriptionId*/);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(insurantKvnr)));
    const auto* language = "de";
    const auto agentName = jwt->stringForClaim(JWT::givenNameClaim).value() + " " +
                           jwt->stringForClaim(JWT::familyNameClaim).value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = std::string(agentName) + " hat eine Liste von Medikament-Informationen heruntergeladen.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::read);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::gkvKvid10);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), insurantKvnr);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_FALSE(entityWhatIdentifierSystem.has_value());
    EXPECT_FALSE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "+");
    EXPECT_EQ(auditEvent.entityWhatReference(), "MedicationDispense");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST(AuditEventCreatorTest, createExpiredTaskDeletion)
{
    const model::AuditEventId eventId = model::AuditEventId::Task_delete_expired_id;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::machine;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }), action, agentType, insurantKvnr, deviceId, prescriptionId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(insurantKvnr)));
    const auto* language = "de";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = "Veraltete E-Rezepte wurden vom Fachdienst automatisch gelöscht.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::del);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_FALSE(agentWhoSystem.has_value());
    EXPECT_FALSE(agentWhoValue.has_value());
    EXPECT_EQ(auditEvent.agentName(), "E-Rezept Fachdienst");
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
    EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityWhatReference(), "Task/" + prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}
