/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/AuditData.hxx"

#include <gtest/gtest.h>


TEST(AuditDataTest, ConstructFromData)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    const model::AuditEventId eventId = model::AuditEventId::GET_Task_id_representative;
    const model::AuditEvent::Action action = model::AuditEvent::Action::update;
    const model::Kvnr insurantKvnr{"X123456788"s, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const std::string consentId = "CHARGCONS-X123456788";
    const std::string_view kvnr = "X424242422";
    const std::string_view agentName = "The Agent Name";
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData(agentName, kvnr), action, agentType, insurantKvnr,
                               deviceId, prescriptionId, consentId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    EXPECT_EQ(eventId, auditData.eventId());
    EXPECT_EQ(action, auditData.action());
    EXPECT_EQ(insurantKvnr, auditData.insurantKvnr());
    EXPECT_EQ(deviceId, auditData.deviceId());
    EXPECT_TRUE(auditData.prescriptionId().has_value());
    EXPECT_EQ(prescriptionId.toString(), auditData.prescriptionId()->toString());
    EXPECT_EQ(consentId, auditData.consentId());
    EXPECT_EQ(auditDataId, auditData.id());
    EXPECT_EQ(recorded, auditData.recorded());
    EXPECT_TRUE(auditData.metaData().agentWho().has_value());
    EXPECT_EQ(kvnr, auditData.metaData().agentWho().value());
    EXPECT_TRUE(auditData.metaData().agentName().has_value());
    EXPECT_EQ(agentName, auditData.metaData().agentName().value());
    EXPECT_EQ(agentType, auditData.agentType());
}


TEST(AuditDataTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Task_abort_doctor;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;

    const model::Kvnr insurantKvnr{std::string{"X123456788"}, model::Kvnr::Type::gkv};
    const std::int16_t deviceId = 5678;

    const model::PrescriptionId prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4242);

    const std::string metaDataJson = R"--(
        {
          "an": "Praxis Dr. Schlunz",
          "aw": "987654321",
          "pz": "ODAyNzY4ODEwMjU1NDg0MzEzMDEwMDAwMDAwMDA2Mzg2ODc4MjAyMjA4MzEwODA3MzY="
        })--";

    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(eventId, model::AuditMetaData::fromJsonNoValidation(metaDataJson), action, agentType,
                               insurantKvnr, deviceId, prescriptionId, std::nullopt);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    EXPECT_EQ(eventId, auditData.eventId());
    EXPECT_EQ(action, auditData.action());
    EXPECT_EQ(insurantKvnr, auditData.insurantKvnr());
    EXPECT_EQ(deviceId, auditData.deviceId());
    EXPECT_TRUE(auditData.prescriptionId().has_value());
    EXPECT_EQ(prescriptionId.toString(), auditData.prescriptionId()->toString());
    EXPECT_FALSE(auditData.consentId().has_value());
    EXPECT_EQ(auditDataId, auditData.id());
    EXPECT_EQ(recorded, auditData.recorded());
    EXPECT_TRUE(auditData.metaData().agentWho().has_value());
    EXPECT_EQ("987654321", auditData.metaData().agentWho().value());
    EXPECT_TRUE(auditData.metaData().agentName().has_value());
    EXPECT_EQ("Praxis Dr. Schlunz", auditData.metaData().agentName().value());
    EXPECT_EQ(agentType, auditData.agentType());
}
