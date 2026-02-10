/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/ProvideDispensation.hxx"
#include "exporter/eventprocessing/EventDispatcher.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "test/exporter/mock/EpaMedicationClientMock.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>

namespace
{
const auto operationOutcomeTpl = R"({
"resourceType": "Parameters",
"id": "example-epa-op-rx-dispensation-erp-output-parameters-1",
"meta": {
    "profile":  [
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-dispensation-erp-output-parameters"
    ]
},
"parameter":  [
    {
        "name": "rxDispensation",
        "part":  [
            {
                "name": "prescriptionId",
                "valueIdentifier": {
                    "system": "https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId",
                    "value": "160.153.303.257.459"
                }
            },
            {
                "name": "authoredOn",
                "valueDate": "2025-01-22"
            },
            {
                "name": "operationOutcome",
                "resource": {
                    "resourceType": "OperationOutcome",
                    "id": "255002c7-aa1b-4163-bdd4-ede482453cca",
                    "meta": {
                        "profile":  [
                            "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-ms-operation-outcome"
                        ]
                    },
                    "issue":  [
                        {
                            "severity": "information",
                            "code": "informational",
                            "details": {
                                "coding":  [
                                    {
                                        "code": "MEDICATIONSVC_OPERATION_SUCCESS",
                                        "system": "https://gematik.de/fhir/epa-medication/CodeSystem/epa-ms-operation-outcome-codes-cs",
                                        "display": "Operation Successfully Completed in Medication Service"
                                    }
                                ]
                            }
                        }
                    ]
                }
            }
        ]
    }
]
})";
}

class ProvideDispensationTest : public testing::Test
{
public:
    model::ProvideDispensationTaskEvent makeEvent(std::optional<ResourceTemplates::Versions::GEM_ERP> gematikVersion)
    {
        const auto lastModified = model::Timestamp::now();
        return model::ProvideDispensationTaskEvent(
            0, model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1),
            model::PrescriptionType::apothekenpflichigeArzneimittel, model::Kvnr{"X12345678"}, "not a real hash",
            model::TaskEvent::UseCase::provideDispensation, model::TaskEvent::State::pending,
            SignedPrescription::fromBinNoVerify(
                CryptoHelper::toCadesBesSignature(std::string(ResourceTemplates::kbvBundleXml()),
                                                  model::Timestamp::now()))
                .getTelematikId(),
            model::TelematikId("doctor-id-not-set"), "doctor organization name not-set",
            std::string(profession_oid::oid_oeffentliche_apotheke), model::TelematikId("pharmacy-not-set"),
            "pharmacy organization name not-set", std::string(profession_oid::oid_oeffentliche_apotheke),
            model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml()),
            model::Bundle::fromXmlNoValidation(
                ResourceTemplates::internal_type::medicationDispenseBundle(
                    ResourceTemplates::MedicationDispenseBundleOptions{
                        .gematikVersion = gematikVersion.value_or(ResourceTemplates::Versions::GEM_ERP_current()),
                        .medicationDispenses{ResourceTemplates::MedicationDispenseOptions{
                            .whenHandedOver = model::Timestamp::now().toGermanDate(),
                            .gematikVersion = gematikVersion.value_or(ResourceTemplates::Versions::GEM_ERP_current()),
                            .medication =
                                ResourceTemplates::MedicationOptions{
                                    .version = gematikVersion
                                                   ? std::variant<std::monostate, ResourceTemplates::Versions::KBV_ERP,
                                                                  ResourceTemplates::Versions::GEM_ERP>{*gematikVersion}
                                                   : std::variant<std::monostate, ResourceTemplates::Versions::KBV_ERP,
                                                                  ResourceTemplates::Versions::GEM_ERP>{}}}}})
                    .serializeToXmlString()),
            lastModified);
    }

    AuditDataCollector auditDataCollector;
};

TEST_F(ProvideDispensationTest, OperationSuccess)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);
    client.setOperationOutcomeResponse(operationOutcomeTpl);

    auto outcome = eventprocessing::ProvideDispensation::process(&client, makeEvent(std::nullopt));
    EXPECT_EQ(outcome, eventprocessing::Outcome::Success);
}

TEST_F(ProvideDispensationTest, OperationSuccess14)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);
    client.setOperationOutcomeResponse(operationOutcomeTpl);

    auto outcome =
        eventprocessing::ProvideDispensation::process(&client, makeEvent(ResourceTemplates::Versions::GEM_ERP_1_4));
    EXPECT_EQ(outcome, eventprocessing::Outcome::Success);
}

TEST_F(ProvideDispensationTest, AuditEvent)
{
    auto client = std::make_unique<EpaMedicationClientMock>();
    client->setHttpStatusResponse(HttpStatus::OK);
    client->setOperationOutcomeResponse(operationOutcomeTpl);

    eventprocessing::EventDispatcher eventDispatcher{std::move(client)};
    AuditDataCollector auditDataCollector;
    ASSERT_NO_FATAL_FAILURE(eventDispatcher.dispatch(makeEvent(std::nullopt), auditDataCollector));
    std::optional<model::AuditData> auditData;
    ASSERT_NO_FATAL_FAILURE(auditData.emplace(auditDataCollector.createData()));
    ASSERT_TRUE(auditData);
    EXPECT_EQ(auditData->agentType(), model::AuditEvent::AgentType::machine);
    EXPECT_EQ(auditData->eventId(), model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP);
    EXPECT_EQ(auditData->action(), model::AuditEvent::Action::create);
    EXPECT_EQ(auditData->insurantKvnr(), model::Kvnr{"X12345678"});
    EXPECT_EQ(auditData->deviceId(), 1);
    EXPECT_EQ(auditData->prescriptionId(),
              model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
}

TEST_F(ProvideDispensationTest, AuditEventPRESCRIPTION_NO_EXIST)
{
    auto client = std::make_unique<EpaMedicationClientMock>();
    client->setHttpStatusResponse(HttpStatus::OK);
    client->setOperationOutcomeResponse(String::replaceAll(operationOutcomeTpl, "MEDICATIONSVC_OPERATION_SUCCESS",
                                                           "MEDICATIONSVC_PRESCRIPTION_NO_EXIST"));

    eventprocessing::EventDispatcher eventDispatcher{std::move(client)};
    AuditDataCollector auditDataCollector;
    eventprocessing::Outcome outcome{};
    ASSERT_NO_FATAL_FAILURE(outcome = eventDispatcher.dispatch(makeEvent(std::nullopt), auditDataCollector));
    std::optional<model::AuditData> auditData;
    ASSERT_TRUE(auditDataCollector.shouldCreateAuditEventOnSuccess());
    EXPECT_EQ(outcome, eventprocessing::Outcome::SuccessAuditFail);
    ASSERT_NO_FATAL_FAILURE(auditData.emplace(auditDataCollector.createData()));
    ASSERT_TRUE(auditData);
    EXPECT_EQ(auditData->agentType(), model::AuditEvent::AgentType::machine);
    EXPECT_EQ(auditData->eventId(), model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed);
    EXPECT_EQ(auditData->action(), model::AuditEvent::Action::create);
    EXPECT_EQ(auditData->insurantKvnr(), model::Kvnr{"X12345678"});
    EXPECT_EQ(auditData->deviceId(), 1);
    EXPECT_EQ(auditData->prescriptionId(),
              model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
}

TEST_F(ProvideDispensationTest, AuditEventPRESCRIPTION_STATUS)
{
    auto client = std::make_unique<EpaMedicationClientMock>();
    client->setHttpStatusResponse(HttpStatus::OK);
    client->setOperationOutcomeResponse(String::replaceAll(operationOutcomeTpl, "MEDICATIONSVC_OPERATION_SUCCESS",
                                                           "MEDICATIONSVC_PRESCRIPTION_STATUS"));

    eventprocessing::EventDispatcher eventDispatcher{std::move(client)};
    AuditDataCollector auditDataCollector;
    eventprocessing::Outcome outcome{};
    ASSERT_NO_FATAL_FAILURE(outcome = eventDispatcher.dispatch(makeEvent(std::nullopt), auditDataCollector));
    std::optional<model::AuditData> auditData;
    ASSERT_TRUE(auditDataCollector.shouldCreateAuditEventOnSuccess());
    EXPECT_EQ(outcome, eventprocessing::Outcome::SuccessAuditFail);
    ASSERT_NO_FATAL_FAILURE(auditData.emplace(auditDataCollector.createData()));
    ASSERT_TRUE(auditData);
    EXPECT_EQ(auditData->agentType(), model::AuditEvent::AgentType::machine);
    EXPECT_EQ(auditData->eventId(), model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed);
    EXPECT_EQ(auditData->action(), model::AuditEvent::Action::create);
    EXPECT_EQ(auditData->insurantKvnr(), model::Kvnr{"X12345678"});
    EXPECT_EQ(auditData->deviceId(), 1);
    EXPECT_EQ(auditData->prescriptionId(),
              model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
}

struct ErrorCodesTestParam {
    std::string errorCode;
    eventprocessing::Outcome expectedOutcome;
};

class ProvideDispensationErrorCodesTestP : public ProvideDispensationTest, public testing::WithParamInterface<ErrorCodesTestParam>
{
public:
    static std::string getName(const testing::TestParamInfo<ErrorCodesTestParam>& info)
    {
        return info.param.errorCode;
    }
};

TEST_P(ProvideDispensationErrorCodesTestP, ProvideDispensationNewErrorCodes)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);

    const auto oo = String::replaceAll(operationOutcomeTpl, "MEDICATIONSVC_OPERATION_SUCCESS", GetParam().errorCode);
    client.setOperationOutcomeResponse(oo);

    auto outcome = eventprocessing::ProvideDispensation::process(&client, makeEvent(std::nullopt));
    EXPECT_EQ(outcome, GetParam().expectedOutcome);
}

INSTANTIATE_TEST_SUITE_P(
    x, ProvideDispensationErrorCodesTestP,
    testing::Values(ErrorCodesTestParam{"MEDSVC_OPERATION_SUCCESS", eventprocessing::Outcome::Success},
                    ErrorCodesTestParam{"MEDICATIONSVC_OPERATION_SUCCESS", eventprocessing::Outcome::Success},
                    ErrorCodesTestParam{"MEDICATIONSVC_NO_VALID_STRUCTURE", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_NO_VALID_STRUCTURE", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDICATIONSVC_PRESCRIPTION_NO_EXIST", eventprocessing::Outcome::SuccessAuditFail},
                    ErrorCodesTestParam{"MEDSVC_PRESCRIPTION_NO_EXIST", eventprocessing::Outcome::SuccessAuditFail},
                    ErrorCodesTestParam{"MEDICATIONSVC_PRESCRIPTION_DUPLICATE", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_PRESCRIPTION_DUPLICATE", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDICATIONSVC_PRESCRIPTION_STATUS", eventprocessing::Outcome::SuccessAuditFail},
                    ErrorCodesTestParam{"MEDSVC_PRESCRIPTION_STATUS", eventprocessing::Outcome::SuccessAuditFail},
                    ErrorCodesTestParam{"MEDICATIONSVC_DISPENSATION_NO_EXIST", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_DISPENSATION_NO_EXIST", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDICATIONSVC_DISPENSATION_STATUS", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_DISPENSATION_STATUS", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDICATIONSVC_PARAMETERS_REFERENCE_NO_EXIST",
                                        eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_PARAMETERS_REFERENCE_NO_EXIST", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_STATUS_INVALID", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_STATEMENT_NO_EXIST", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_PARAMETERS_INVALID_CONTENT", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_DOSAGE_INVALID", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_EMP_CHRONOLOGY_ID_MISMATCH", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_OPERATION_OUTSIDE_BATCH", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_ALREADY_LINKED", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_EMP_ENTRY_ALREADY_EXISTS", eventprocessing::Outcome::DeadLetter},
                    ErrorCodesTestParam{"MEDSVC_EMP_NO_EXIST", eventprocessing::Outcome::DeadLetter}),
    &ProvideDispensationErrorCodesTestP::getName);
