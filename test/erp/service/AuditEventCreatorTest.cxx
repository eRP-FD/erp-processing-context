/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/audit/AuditEventCreator.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "shared/audit/AuditEventTextTemplates.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


class AuditEventCreatorTest : public testing::Test
{
public:
    void SetUp() override{};
};

TEST_F(AuditEventCreatorTest, createRepresentative)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_Task_id_representative;
    const model::AuditEvent::Action action = model::AuditEvent::Action::read;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const std::string_view kvnr = "X424242422";
    const std::string_view agentName = "Max Mustermann";
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData(agentName, kvnr), action, agentType, insurantKvnr,
                               deviceId, prescriptionId, std::nullopt);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(kvnr)));
    const auto* language = "de";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
        " hat das Rezept mit der ID " + prescriptionId.toString() + " heruntergeladen.</div>";
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


TEST_F(AuditEventCreatorTest, createPharmacyGetAllTasksWithPnwPzNumberEn)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pz,
        model::AuditMetaData(agentName, telematikId),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv},
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "en", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " retrieved your dispensable e-prescriptions using your health card.</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST_F(AuditEventCreatorTest, createPharmacyGetAllTasksWithPnwPzNumberDe)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pz,
        model::AuditMetaData(agentName, telematikId),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv},
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " hat mit Ihrer eGK die Liste der offenen E-Rezepte abgerufen.</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST_F(AuditEventCreatorTest, createPharmacyGetAllTasksWithInvalidPnw)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed,
        model::AuditMetaData(agentName, telematikId),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv},
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    {
        const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);
        const std::string textDiv =
            "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
            " konnte aufgrund eines Fehlerfalls nicht die Liste der offenen E-Rezepte mit Ihrer eGK abrufen."
            "</div>";
        EXPECT_EQ(auditEvent.textDiv(), textDiv);
    }
    {
        const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "en", textResources, *jwt);
        const std::string textDiv =
            "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
            " was not able to retrieve your e-prescriptions due to an error with your health card."
            "</div>";
        EXPECT_EQ(auditEvent.textDiv(), textDiv);
    }
}

TEST_F(AuditEventCreatorTest, createPharmacyGetAllTasksWithPn3)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3,
        model::AuditMetaData(agentName, telematikId),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv},
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " hat mit Ihrer eGK die Liste der offenen E-Rezepte abgerufen. (Offline-Check wurde akzeptiert)</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST_F(AuditEventCreatorTest, createPharmacyGetAllTasksWithPn3Failed)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed,
        model::AuditMetaData(agentName, telematikId),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv},
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    {
        const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);
        const std::string textDiv =
            "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
            " konnte aufgrund eines Fehlerfalls nicht die Liste der offenen E-Rezepte mit Ihrer eGK abrufen."
            " (Offline-Check wurde nicht akzeptiert)</div>";
        EXPECT_EQ(auditEvent.textDiv(), textDiv);
    }
}

TEST_F(AuditEventCreatorTest, createPatient)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Task_abort_insurant;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId,
                               prescriptionId, std::nullopt);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr.id()));
    const auto* language = "en";
    const auto agentName = jwt->displayName().value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " deleted a prescription " + prescriptionId.toString() + ".</div>";
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
    EXPECT_EQ(auditEvent.entityWhatReference(), "Task/"+prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST_F(AuditEventCreatorTest, createGetMultipleResources)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_MedicationDispense;
    const model::AuditEvent::Action action = model::AuditEvent::Action::read;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId, {}, {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "de";
    const auto agentName = jwt->displayName().value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = std::string(agentName) + " hat eine Liste mit Medikament-Informationen heruntergeladen.";
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

TEST_F(AuditEventCreatorTest, createExpiredTaskDeletion)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::Task_delete_expired_id;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::machine;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);

    model::AuditData auditData(
        eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId, prescriptionId, {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "de";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = "Veraltete Rezepte wurden vom Fachdienst automatisch gelöscht.";
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

TEST_F(AuditEventCreatorTest, createExpiredCommunicationDeletion)
{
    const model::AuditEventId eventId = model::AuditEventId::Communication_delete_expired_id;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::machine;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId, {},
                               {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    const AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "de";
    const model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = "Veraltete Nachrichten wurden vom Fachdienst automatisch gelöscht.";
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
    ASSERT_FALSE(entityWhatIdentifierSystem.has_value());
    EXPECT_FALSE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "+");
    EXPECT_EQ(auditEvent.entityWhatReference(), "Communication");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST_F(AuditEventCreatorTest, createPostChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_ChargeItem;
    const model::AuditEvent::Action action = model::AuditEvent::Action::create;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const std::string_view telematikId = "2-2-ERP-AKTOR-ZArzt-01";
    const model::Timestamp recorded = model::Timestamp::now();
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 4241);
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(std::string(telematikId)));
    const auto agentName = jwt->stringForClaim(JWT::organizationNameClaim).value();

    model::AuditData auditData(eventId, model::AuditMetaData(agentName, telematikId), action, agentType, insurantKvnr,
                               deviceId, prescriptionId, {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto* language = "de";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = std::string(agentName) + " hat Abrechnungsinformationen zu dem Rezept mit der ID "+
        prescriptionId.toString() + " gespeichert.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::create);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::telematicID);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), telematikId);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
    EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityWhatReference(), "ChargeItem/" + prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST_F(AuditEventCreatorTest, createPutChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::PUT_ChargeItem_id;
    const model::AuditEvent::Action action = model::AuditEvent::Action::update;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::pkv};
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241);
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const std::string_view telematikId = "2-2-ERP-AKTOR-ZArzt-01";
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(std::string(telematikId)));
    const auto agentName = jwt->stringForClaim(JWT::organizationNameClaim).value();
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData(agentName, telematikId), action, agentType, insurantKvnr, deviceId, prescriptionId, {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto* language = "en";
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
        " changed charging information for a prescription " + prescriptionId.toString() + ".</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::update);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);

    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem, model::resource::naming_system::telematicID);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), telematikId);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
    EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityWhatReference(), "ChargeItem/" + prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST_F(AuditEventCreatorTest, createDeleteConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::DELETE_Consent;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const auto insurantKvnr = model::Kvnr{"X123456788", model::Kvnr::Type::pkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }), action, agentType, insurantKvnr, deviceId, {}, "CHARGCONS-X123456788");
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "de";
    const auto agentName = jwt->displayName().value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = std::string(agentName) + " hat die Einwilligung widerrufen.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::del);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::pkvKvid10);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), insurantKvnr);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_FALSE(entityWhatIdentifierSystem.has_value());
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "CHARGCONS");
    EXPECT_EQ(auditEvent.entityWhatReference(), "Consent/CHARGCONS-X123456788");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST_F(AuditEventCreatorTest, createPostConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Consent;
    const model::AuditEvent::Action action = model::AuditEvent::Action::create;
    const auto insurantKvnr = model::Kvnr{"X123456788", model::Kvnr::Type::pkv};
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId, {}, "CHARGCONS-X123456788");
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "de";
    const auto agentName = jwt->displayName().value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), language);
    const std::string text = std::string(agentName) + " hat die Einwilligung erteilt.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::create);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);
    const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
    EXPECT_TRUE(agentWhoSystem.has_value());
    EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::pkvKvid10);
    EXPECT_TRUE(agentWhoValue.has_value());
    EXPECT_EQ(agentWhoValue.value(), insurantKvnr);
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), "Device/" + std::to_string(deviceId));
    const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
    EXPECT_FALSE(entityWhatIdentifierSystem.has_value());
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "CHARGCONS");
    EXPECT_EQ(auditEvent.entityWhatReference(), "Consent/CHARGCONS-X123456788");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}



TEST_F(AuditEventCreatorTest, createWithUnsupportedLanguage)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Task_abort_insurant;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const auto insurantKvnr = model::Kvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData({}, {}), action, agentType, insurantKvnr, deviceId,
                               prescriptionId, std::nullopt);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(insurantKvnr));
    const auto* language = "fr";  // not supported, will use default language "en";
    const auto agentName = jwt->displayName().value();
    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(auditData, language, textResources, *jwt);

    EXPECT_EQ(auditEvent.id(), auditDataId);
    EXPECT_EQ(auditEvent.language(), "en");
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " deleted a prescription " + prescriptionId.toString() + ".</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}
