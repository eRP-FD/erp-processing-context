/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/AuditEvent.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/FileHelper.hxx"
#include "test_config.h"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>


TEST(AuditEventTest, ConstructFromData)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string_view id = "auditevent_id";
    const std::string_view language = "de";
    const std::string_view textDiv = "<div xmlns=\"http://www.w3.org/1999/xhtml\">Praxis Dr. Frankenstein hat ein E-Rezept eingestellt.</div>";
    const auto subTypeCode = model::AuditEvent::SubType::update;
    const auto action = model::AuditEvent::Action::update;
    const auto recorded = model::Timestamp::now();
    const auto outcome = model::AuditEvent::Outcome::success;
    const std::string_view whoIdentifierValue = "3-SMC-XXXX-883110000120312";
    const std::string_view agentName = "Praxis Dr. Frankenstein";
    const auto agentType = model::AuditEvent::AgentType::machine;  // for test;
    const std::string_view sourceObserverReference = "Device/1234";
    const std::string_view whatIdentifierValue = "160.100.000.000.000.42";
    const std::string whatReference = "Task/" + std::string(whatIdentifierValue) + "/$activate";
    const std::string_view entityName = "X123456788";
    const auto entityDescription = std::string(whatIdentifierValue) + "*";

    model::AuditEvent auditEvent;
    auditEvent.setId(id);
    auditEvent.setLanguage(language);
    auditEvent.setTextDiv(textDiv);
    auditEvent.setSubTypeCode(subTypeCode);
    auditEvent.setAction(action);
    auditEvent.setRecorded(recorded);
    auditEvent.setOutcome(outcome);
    auditEvent.setAgentWho(model::resource::naming_system::telematicID, whoIdentifierValue);
    auditEvent.setAgentName(agentName);
    auditEvent.setAgentType(agentType);
    auditEvent.setSourceObserverReference(sourceObserverReference);
    auditEvent.setEntityWhatIdentifier(model::resource::naming_system::prescriptionID, whatIdentifierValue);
    auditEvent.setEntityWhatReference(whatReference);
    auditEvent.setEntityName(entityName);
    auditEvent.setEntityDescription(entityDescription);

    EXPECT_EQ(auditEvent.id(), id);
    EXPECT_EQ(auditEvent.language(), language);
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), subTypeCode);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), outcome);
    EXPECT_EQ(auditEvent.agentWho(), std::tie(model::resource::naming_system::telematicID, whoIdentifierValue));
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), sourceObserverReference);
    EXPECT_EQ(auditEvent.entityWhatIdentifier(),
              std::tie(model::resource::naming_system::prescriptionID, whatIdentifierValue));
    EXPECT_EQ(auditEvent.entityWhatReference(), whatReference);
    EXPECT_EQ(auditEvent.entityName(), entityName);
    EXPECT_EQ(auditEvent.entityDescription(), entityDescription);
}


TEST(AuditEventTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    const auto gematikVersion = ResourceTemplates::Versions::GEM_ERP_current();
    const std::string_view id = "01eb69a4-9029-d610-b1cf-ddb8c6c0bfbc";
    const std::string_view language = "de";
    const std::string_view whatIdentifierValue = "160.100.000.000.000.42";
    const std::string textDiv =
        "<div xmlns=\"http://www.w3.org/1999/xhtml\">Praxis Dr. Frankenstein hat das Rezept mit der ID " + std::string(whatIdentifierValue) + " eingestellt.</div>";
    const auto subTypeCode = model::AuditEvent::SubType::update;
    const auto action = model::AuditEvent::Action::update;
    const auto recorded = model::Timestamp::fromXsDateTime("2021-03-15T15:24:38.396+00:00");
    const auto outcome = model::AuditEvent::Outcome::success;
    const std::string_view whoIdentifierValue = "3-SMC-XXXX-883110000120312";
    const std::string_view agentName = "Praxis Dr. Frankenstein";
    const auto agentType = model::AuditEvent::AgentType::human;
    const std::string_view sourceObserverReference = "Device/1234";
    const std::string whatReference = "Task/" + std::string(whatIdentifierValue);
    const std::string_view entityName = "X122446685";
    const auto entityDescription = whatIdentifierValue;

    const std::string gematikVersionStr{to_string(gematikVersion)};
    const std::string auditEventFileName =
        std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/audit_event_" + gematikVersionStr + ".json";
    const auto json = FileHelper::readFileAsString(auditEventFileName);
    const auto auditEvent = model::AuditEvent::fromJsonNoValidation(json);

    EXPECT_EQ(auditEvent.id(), id);
    EXPECT_EQ(auditEvent.language(), language);
    EXPECT_EQ(auditEvent.textDiv(), textDiv);
    EXPECT_EQ(auditEvent.subTypeCode(), subTypeCode);
    EXPECT_EQ(auditEvent.action(), action);
    EXPECT_EQ(auditEvent.recorded(), recorded);
    EXPECT_EQ(auditEvent.outcome(), outcome);
    EXPECT_EQ(auditEvent.agentWho(), std::tie(model::resource::naming_system::telematicID, whoIdentifierValue));
    EXPECT_EQ(auditEvent.agentName(), agentName);
    EXPECT_EQ(auditEvent.agentType(), agentType);
    EXPECT_EQ(auditEvent.sourceObserverReference(), sourceObserverReference);
    EXPECT_EQ(auditEvent.entityWhatIdentifier(),
              std::tie(model::resource::naming_system::prescriptionID, whatIdentifierValue));
    EXPECT_EQ(auditEvent.entityWhatReference(), whatReference);
    EXPECT_EQ(auditEvent.entityName(), entityName);
    EXPECT_EQ(auditEvent.entityDescription(), entityDescription);
}
