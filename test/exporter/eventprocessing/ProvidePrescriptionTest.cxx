/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/ProvidePrescription.hxx"
#include "exporter/eventprocessing/EventDispatcher.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "test/exporter/mock/EpaMedicationClientMock.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>

class ProvidePrescriptionTest : public testing::Test
{
public:
    model::ProvidePrescriptionTaskEvent makeEvent()
    {
        const auto lastModified = model::Timestamp::now();
        return model::ProvidePrescriptionTaskEvent(
            0, model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1),
            model::PrescriptionType::apothekenpflichigeArzneimittel, model::Kvnr{"X12345678"}, "not a real hash",
            model::TaskEvent::UseCase::providePrescription, model::TaskEvent::State::pending,
            SignedPrescription::fromBinNoVerify(
                CryptoHelper::toCadesBesSignature(std::string(ResourceTemplates::kbvBundleXml()),
                                                  model::Timestamp::now()))
                .getTelematikId(),
            model::TelematikId("doctor id not set"), "doctor organization name not-set",
            std::string(profession_oid::oid_arzt),
            model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml()), lastModified);
    }

    AuditDataCollector auditDataCollector;
};

TEST_F(ProvidePrescriptionTest, OperationSuccess)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);
    client.setOperationOutcomeResponse(R"({
"resourceType": "Parameters",
"id": "example-epa-op-rx-prescription-erp-output-parameters-1",
"meta": {
    "profile":  [
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-prescription-erp-output-parameters"
    ]
},
"parameter":  [
    {
        "name": "rxPrescription",
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
})");

    auto outcome = eventprocessing::ProvidePrescription::process(&client, makeEvent());
    EXPECT_EQ(outcome, eventprocessing::Outcome::Success);
}

TEST_F(ProvidePrescriptionTest, PrescriptionDuplicate)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);
    client.setOperationOutcomeResponse(R"({
"resourceType": "Parameters",
"id": "example-epa-op-rx-prescription-erp-output-parameters-1",
"meta": {
    "profile":  [
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-prescription-erp-output-parameters"
    ]
},
"parameter":  [
    {
        "name": "rxPrescription",
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
                                        "code": "MEDICATIONSVC_PRESCRIPTION_DUPLICATE",
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
})");
    auto outcome = eventprocessing::ProvidePrescription::process(&client, makeEvent());
    EXPECT_EQ(outcome, eventprocessing::Outcome::Success);
}


TEST_F(ProvidePrescriptionTest, PrescriptionStatus)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::OK);
    client.setOperationOutcomeResponse(R"({
"resourceType": "Parameters",
"id": "example-epa-op-rx-prescription-erp-output-parameters-1",
"meta": {
    "profile":  [
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-prescription-erp-output-parameters"
    ]
},
"parameter":  [
    {
        "name": "rxPrescription",
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
                                        "code": "MEDICATIONSVC_PRESCRIPTION_STATUS",
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
})");

    auto outcome = eventprocessing::ProvidePrescription::process(&client, makeEvent());
    EXPECT_EQ(outcome, eventprocessing::Outcome::Success);
}

TEST_F(ProvidePrescriptionTest, NoValidStructure)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::BadRequest);
    client.setOperationOutcomeResponse(R"({
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
                    "code": "MEDICATIONSVC_NO_VALID_STRUCTURE",
                    "system": "https://gematik.de/fhir/epa-medication/CodeSystem/epa-ms-operation-outcome-codes-cs",
                    "display": "Operation Successfully Completed in Medication Service"
                }
            ]
        }
    }
]
})");

    auto outcome = eventprocessing::ProvidePrescription::process(&client, makeEvent());
    EXPECT_EQ(outcome, eventprocessing::Outcome::DeadLetter);
}

TEST_F(ProvidePrescriptionTest, InternalServerError)
{
    EpaMedicationClientMock client;
    client.setHttpStatusResponse(HttpStatus::InternalServerError);
    client.setOperationOutcomeResponse(R"({
"errorCode": "internalError"
})");

    auto outcome = eventprocessing::ProvidePrescription::process(&client, makeEvent());
    EXPECT_EQ(outcome, eventprocessing::Outcome::Retry);
}

TEST_F(ProvidePrescriptionTest, AuditEvent)
{
    auto client = std::make_unique<EpaMedicationClientMock>();
    client->setHttpStatusResponse(HttpStatus::OK);
    client->setOperationOutcomeResponse(R"({
"resourceType": "Parameters",
"id": "example-epa-op-rx-prescription-erp-output-parameters-1",
"meta": {
    "profile":  [
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-prescription-erp-output-parameters"
    ]
},
"parameter":  [
    {
        "name": "rxPrescription",
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
})");

    eventprocessing::EventDispatcher eventDispatcher{std::move(client)};
    AuditDataCollector auditDataCollector;
    ASSERT_NO_FATAL_FAILURE(eventDispatcher.dispatch(makeEvent(), auditDataCollector));
    std::optional<model::AuditData> auditData;
    ASSERT_NO_FATAL_FAILURE(auditData.emplace(auditDataCollector.createData()));
    ASSERT_TRUE(auditData);
    EXPECT_EQ(auditData->agentType(), model::AuditEvent::AgentType::machine);
    EXPECT_EQ(auditData->eventId(), model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP);
    EXPECT_EQ(auditData->action(), model::AuditEvent::Action::create);
    EXPECT_EQ(auditData->insurantKvnr(), model::Kvnr{"X12345678"});
    EXPECT_EQ(auditData->deviceId(), 1);
    EXPECT_EQ(auditData->prescriptionId(),
              model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
}
