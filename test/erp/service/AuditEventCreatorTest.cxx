/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/AuditEventCreator.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/model/ResourceNames.hxx"
#include "test/util/JwtBuilder.hxx"

#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

namespace
{
    constexpr std::string_view pnwPzNumber = "ODAyNzY4ODEwMjU1NDg0MzEzMDEwMDAwMDAwMDA2Mzg2ODc4MjAyMjA4MzEwODA3MzY=";
}

TEST(AuditEventCreatorTest, createRepresentative)//NOLINT(readability-function-cognitive-complexity)
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

    model::AuditData auditData(eventId, model::AuditMetaData(agentName, kvnr, std::nullopt), action, agentType, insurantKvnr,
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

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWrongTypeWithPnwPzNumber) //NOLINT(readability-function-cognitive-complexity)
{
    const std::string telematikId{"12345654321"};

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pz,
        model::AuditMetaData("Test Name", telematikId, std::nullopt),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));

    try
    {
        static_cast<void>(AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt));
        FAIL();
    }
    catch (const std::logic_error& ex)
    {
        EXPECT_EQ(
            std::string{ex.what()},
            "PNW PZ number should be present if and only if event ID is GET_Tasks_by_pharmacy_with_pz");
    }
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWrongTypeWithoutPnwPzNumber) //NOLINT(readability-function-cognitive-complexity)
{
    const std::string telematikId{"12345654321"};

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_without_pz,
        model::AuditMetaData("Test Name", telematikId, pnwPzNumber),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));

    try
    {
        static_cast<void>(AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt));
        FAIL();
    }
    catch (const std::logic_error& ex)
    {
        EXPECT_EQ(
            std::string{ex.what()},
            "PNW PZ number should be present if and only if event ID is GET_Tasks_by_pharmacy_with_pz");
    }
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWithPnwPzNumberEn)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pz,
        model::AuditMetaData(agentName, telematikId, pnwPzNumber),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "en", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " retrieved your dispensable e-prescriptions using your health card"
                                " (check number: " + pnwPzNumber.data() + ").</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWithPnwPzNumberDe)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_with_pz,
        model::AuditMetaData(agentName, telematikId, pnwPzNumber),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " hat mit Ihrer Gesundheitskarte alle Ihre einlösbaren E-Rezepte abgerufen"
                                " (Prüfziffer: " + pnwPzNumber.data() + ").</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWithoutPnwPzNumberEn)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_without_pz,
        model::AuditMetaData(agentName, telematikId, std::nullopt),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "en", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " retrieved your dispensable e-prescriptions using your health card."
                                " (no check number available).</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWithoutPnwPzNumberDe)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_without_pz,
        model::AuditMetaData(agentName, telematikId, std::nullopt),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);

    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
                                " hat mit Ihrer Gesundheitskarte alle Ihre einlösbaren E-Rezepte abgerufen."
                                " (Keine Prüfziffer vorhanden)</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
}

TEST(AuditEventCreatorTest, createPharmacyGetAllTasksWithInvalidPnw)
{
    const std::string telematikId{"12345654321"};
    const std::string_view agentName = "Test Name";

    model::AuditData auditData(
        model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed,
        model::AuditMetaData(agentName, telematikId, std::nullopt),
        model::AuditEvent::Action::read,
        model::AuditEvent::AgentType::human,
        "X123456789",
        1234,
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4241),
        std::nullopt);

    auditData.setId("audit_data_id");
    auditData.setRecorded(model::Timestamp::now());

    AuditEventTextTemplates textResources{};
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(telematikId));
    {
        const auto auditEvent = AuditEventCreator::fromAuditData(auditData, "de", textResources, *jwt);
        const std::string textDiv =
            "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + std::string(agentName) +
            " konnte aufgrund eines Fehlers Ihre E-Rezepte nicht mit Ihrer Gesundheitskarte abrufen."
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


TEST(AuditEventCreatorTest, createPatient)//NOLINT(readability-function-cognitive-complexity)
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

    model::AuditData auditData(eventId, model::AuditMetaData({}, {}, {}), action, agentType, insurantKvnr, deviceId,
                               prescriptionId, std::nullopt);
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
    EXPECT_EQ(auditEvent.entityWhatReference(), "Task/"+prescriptionId.toString()+ "/$abort");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST(AuditEventCreatorTest, createGetMultipleResources)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_MedicationDispense;
    const model::AuditEvent::Action action = model::AuditEvent::Action::read;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({}, {}, {}), action, agentType, insurantKvnr, deviceId, {}, {});
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

TEST(AuditEventCreatorTest, createExpiredTaskDeletion)//NOLINT(readability-function-cognitive-complexity)
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
        eventId, model::AuditMetaData({}, {}, {}), action, agentType, insurantKvnr, deviceId, prescriptionId, {});
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    AuditEventTextTemplates textResources;
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtVersicherter(std::string(insurantKvnr)));
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

TEST(AuditEventCreatorTest, createPostChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_ChargeItem;
    const model::AuditEvent::Action action = model::AuditEvent::Action::create;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const std::string_view telematikId = "2-2-ERP-AKTOR-ZArzt-01";
    const model::Timestamp recorded = model::Timestamp::now();
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const auto jwt = std::make_unique<JWT>(JwtBuilder::testBuilder().makeJwtApotheke(std::string(telematikId)));
    const auto agentName = jwt->stringForClaim(JWT::organizationNameClaim).value();

    model::AuditData auditData(eventId, model::AuditMetaData(agentName, telematikId, std::nullopt), action, agentType, insurantKvnr,
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

TEST(AuditEventCreatorTest, createPutChargeItemPatient)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::PUT_ChargeItem_id_insurant;
    const model::AuditEvent::Action action = model::AuditEvent::Action::update;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }, { }), action, agentType, insurantKvnr, deviceId, prescriptionId, {});
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
        " changed marking in charging information for a prescription " + prescriptionId.toString() + ".</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::update);
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
    EXPECT_EQ(auditEvent.entityWhatReference(), "ChargeItem/" + prescriptionId.toString());
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST(AuditEventCreatorTest, createDeleteConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::DELETE_Consent;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({ }, { }, { }), action, agentType, insurantKvnr, deviceId, {}, "CHARGCONS-X123456789");
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
    const std::string text = std::string(agentName) + " hat die Einwilligung widerrufen.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::del);
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
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "CHARGCONS");
    EXPECT_EQ(auditEvent.entityWhatReference(), "Consent/CHARGCONS-X123456789");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

TEST(AuditEventCreatorTest, createPostConsent)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Consent;
    const model::AuditEvent::Action action = model::AuditEvent::Action::create;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData({}, {}, {}), action, agentType, insurantKvnr, deviceId, {}, "CHARGCONS-X123456789");
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
    const std::string text = std::string(agentName) + " hat die Einwilligung erteilt.";
    const std::string textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">" + text + "</div>";
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), model::AuditEvent::SubType::create);
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
    EXPECT_TRUE(entityWhatIdentifierValue.has_value());
    EXPECT_EQ(auditEvent.entityDescription(), "CHARGCONS");
    EXPECT_EQ(auditEvent.entityWhatReference(), "Consent/CHARGCONS-X123456789");
    EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

