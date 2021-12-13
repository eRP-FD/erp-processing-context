/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Composition.hxx"

#include <gtest/gtest.h>


TEST(CompositionTest, ConstructFromData)
{
    const std::string_view telematicId = "12345654321";
    const model::Timestamp start = model::Timestamp::fromXsDateTime("2021-02-02T17:13:00+01:00");
    const model::Timestamp end = model::Timestamp::fromXsDateTime("2021-02-05T11:12:00+01:00");
    const std::string_view author = "https://prescriptionserver.telematik/Device/ErxService";
    const std::string_view prescriptionDigestIdentifier = "Binary/PrescriptionDigest-160.000.080.761.527.39";

    const model::Composition composition(telematicId, start, end, author, prescriptionDigestIdentifier);

    EXPECT_TRUE(composition.telematikId().has_value());
    EXPECT_EQ(composition.telematikId().value(), telematicId);

    EXPECT_TRUE(composition.periodStart().has_value());
    EXPECT_EQ(composition.periodStart().value(), start);

    EXPECT_TRUE(composition.periodEnd().has_value());
    EXPECT_EQ(composition.periodEnd().value(), end);

    EXPECT_TRUE(composition.date().has_value());
    EXPECT_EQ(composition.date().value(), end);

    EXPECT_TRUE(composition.author().has_value());
    EXPECT_EQ(composition.author().value(), author);

    EXPECT_TRUE(composition.prescriptionDigestIdentifier().has_value());
    EXPECT_EQ(composition.prescriptionDigestIdentifier().value(), prescriptionDigestIdentifier);
}


TEST(CompositionTest, ConstructFromJson)
{
    const std::string json = R"(
{
    "resourceType":"Composition",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxComposition"
        ]
    },
    "extension":[
        {
            "url":"https://gematik.de/fhir/StructureDefinition/BeneficiaryExtension",
            "valueIdentifier":{
                "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                "value":"987654321"
            }
        },
        {
            "url": "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis",
            "valueCoding": {
              "system": "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN",
              "code": "04"
            }
        }
    ],
    "status":"final",
    "type":{
        "coding":[
            {
                "system":"https://gematik.de/fhir/CodeSystem/Documenttype",
                "code":"3",
                "display":"Receipt"
            }
        ]
    },
    "date":"2021-03-20T07:32:34.328+00:00",
    "author":[
        {
            "reference":"https://prescriptionserver.telematik/Device/ErxService"
        }
    ],
    "title":"Quittung",
    "event":[
        {
            "period":{
                "start":"2021-01-20T07:23:34.328+00:00",
                "end":"2021-01-20T07:31:34.328+00:00"
            }
        }
    ]
}
)";

    const auto composition = model::Composition::fromJsonNoValidation(json);

    const std::string telematicId = "987654321";
    const model::Timestamp start = model::Timestamp::fromXsDateTime("2021-01-20T07:23:34.328+00:00");
    const model::Timestamp end = model::Timestamp::fromXsDateTime("2021-01-20T07:31:34.328+00:00");
    const model::Timestamp date = model::Timestamp::fromXsDateTime("2021-03-20T07:32:34.328+00:00");
    const std::string_view author = "https://prescriptionserver.telematik/Device/ErxService";

    EXPECT_TRUE(composition.telematikId().has_value());
    EXPECT_EQ(composition.telematikId().value(), telematicId);

    EXPECT_TRUE(composition.periodStart().has_value());
    EXPECT_EQ(composition.periodStart().value(), start);

    EXPECT_TRUE(composition.periodEnd().has_value());
    EXPECT_EQ(composition.periodEnd().value(), end);

    EXPECT_TRUE(composition.date().has_value());
    EXPECT_EQ(composition.date().value(), date);

    EXPECT_TRUE(composition.author().has_value());
    EXPECT_EQ(composition.author().value(), author);

    ASSERT_TRUE(composition.legalBasisCode().has_value());
    EXPECT_EQ(composition.legalBasisCode().value(),
              model::KbvStatusKennzeichen::entlassmanagementKennzeichen);
}
