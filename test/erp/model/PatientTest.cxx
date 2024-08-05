/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Kvnr.hxx"
#include "erp/model/Patient.hxx"

#include <gtest/gtest.h>

TEST(PatientTest, getKvnr)
{
    constexpr std::string_view patientJson = R"--({
        "resourceType": "Patient",
        "id": "9774f67f-a238-4daf-b4e6-679deeef3811",
        "meta": {
          "profile": [
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.0.3"
          ]
        },
        "identifier": [
          {
            "type": {
              "coding": [
                {
                  "system": "http://fhir.de/CodeSystem/identifier-type-de-basis",
                  "code": "GKV"
                }
              ]
            },
            "system": "http://fhir.de/NamingSystem/gkv/kvid-10",
            "value": "X234567891"
          }
        ],
        "name": [
          {
            "use": "official",
            "family": "Ludger Königsstein",
            "_family": {
              "extension": [
                {
                  "url": "http://hl7.org/fhir/StructureDefinition/humanname-own-name",
                  "valueString": "Königsstein"
                }
              ]
            },
            "given": [
              "Ludger"
            ]
          }
        ],
        "birthDate": "1935-06-22",
        "address": [
          {
            "type": "both",
            "line": [
              "Musterstr. 1"
            ],
            "_line": [
              {
                "extension": [
                  {
                    "url": "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName",
                    "valueString": "Musterstr."
                  },
                  {
                    "url": "http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber",
                    "valueString": "1"
                  }
                ]
              }
            ],
            "city": "Berlin",
            "postalCode": "10623"
          }
        ]
      })--";

    const auto patient = model::Patient::fromJsonNoValidation(patientJson);

    ASSERT_EQ(patient.kvnr(), "X234567891");
}
