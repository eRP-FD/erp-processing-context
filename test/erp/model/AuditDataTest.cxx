#include "erp/model/AuditData.hxx"

#include <gtest/gtest.h>


TEST(AuditDataTest, ConstructFromData)
{
    const model::AuditEventId eventId = model::AuditEventId::GET_Task_id_representative;
    const model::AuditEvent::Action action = model::AuditEvent::Action::update;
    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 1234;
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4241);
    const std::string_view kvnr = "X424242424";
    const std::string_view agentName = "The Agent Name";
    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData(agentName, kvnr), action, agentType, insurantKvnr, deviceId, prescriptionId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    EXPECT_EQ(eventId, auditData.eventId());
    EXPECT_EQ(action, auditData.action());
    EXPECT_EQ(insurantKvnr, auditData.insurantKvnr());
    EXPECT_EQ(deviceId, auditData.deviceId());
    EXPECT_TRUE(auditData.prescriptionId().has_value());
    EXPECT_EQ(prescriptionId.toString(), auditData.prescriptionId()->toString());
    EXPECT_EQ(auditDataId, auditData.id());
    EXPECT_EQ(recorded, auditData.recorded());
    EXPECT_TRUE(auditData.metaData().agentWho().has_value());
    EXPECT_EQ(kvnr, auditData.metaData().agentWho().value());
    EXPECT_TRUE(auditData.metaData().agentName().has_value());
    EXPECT_EQ(agentName, auditData.metaData().agentName().value());
    EXPECT_EQ(agentType, auditData.agentType());
}


TEST(AuditDataTest, ConstructFromJson)
{
    const model::AuditEventId eventId = model::AuditEventId::POST_Task_abort_doctor;
    const model::AuditEvent::Action action = model::AuditEvent::Action::del;

    const std::string insurantKvnr = "X123456789";
    const std::int16_t deviceId = 5678;

    const model::PrescriptionId prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4242);

    const std::string metaDataJson = R"--(
        {
          "an": "Praxis Dr. Schlunz",
          "aw": "987654321"
        })--";

    const model::AuditEvent::AgentType agentType = model::AuditEvent::AgentType::human;
    const std::string auditDataId = "audit_data_id";
    const model::Timestamp recorded = model::Timestamp::now();

    model::AuditData auditData(
        eventId, model::AuditMetaData::fromJson(metaDataJson), action, agentType, insurantKvnr, deviceId, prescriptionId);
    auditData.setId(auditDataId);
    auditData.setRecorded(recorded);

    EXPECT_EQ(eventId, auditData.eventId());
    EXPECT_EQ(action, auditData.action());
    EXPECT_EQ(insurantKvnr, auditData.insurantKvnr());
    EXPECT_EQ(deviceId, auditData.deviceId());
    EXPECT_TRUE(auditData.prescriptionId().has_value());
    EXPECT_EQ(prescriptionId.toString(), auditData.prescriptionId()->toString());
    EXPECT_EQ(auditDataId, auditData.id());
    EXPECT_EQ(recorded, auditData.recorded());
    EXPECT_TRUE(auditData.metaData().agentWho().has_value());
    EXPECT_EQ("987654321", auditData.metaData().agentWho().value());
    EXPECT_TRUE(auditData.metaData().agentName().has_value());
    EXPECT_EQ("Praxis Dr. Schlunz", auditData.metaData().agentName().value());
    EXPECT_EQ(agentType, auditData.agentType());
}
