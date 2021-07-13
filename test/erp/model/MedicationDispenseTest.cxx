#include "erp/model/MedicationDispense.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


TEST(MedicationDispenseTest, WrongSchemaMissingIdentifier)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyIdentifiers)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/Covid-19",
            "value":"456"
        },
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier1)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "value":"456"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier2)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/Covid-19"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier3)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/Covid-19",
            "value":"none"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingSubject)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManySubjects)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        },
        "identifier":{
            "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
            "value":"0815"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongSubject)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
            "value":"0815"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingPerformer)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyPerformers)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/System1",
                    "value":"value1"
                }
            }
        },
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/System2",
                    "value":"value27"
                }
            }
        },
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongPerformer)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
                    "value":"X123456789"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingWhenHandedOver)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense), ErpException);
}

TEST(MedicationDispenseTest, CorrectSchema)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
                    "value":"1111"
                }
            }
        }
    ],
    "whenHandedOver":"2020-03-20T07:13:00+05:00",
    "dosageInstruction":[
        {
            "text":"1-0-1-0"
        }
    ]
}
)";

    auto medicationDispense = model::MedicationDispense::fromJson(json);

    EXPECT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense));

    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");
    medicationDispense.setId(prescriptionId);
    EXPECT_EQ(medicationDispense.id().toString(), prescriptionId.toString());

    const std::string kvnr = "X424242424";
    medicationDispense.setKvnr(kvnr);
    EXPECT_EQ(medicationDispense.kvnr(), kvnr);

    const std::string telematicId = "1234567";
    medicationDispense.setTelematicId(telematicId);
    EXPECT_EQ(medicationDispense.telematikId(), telematicId);

    const model::Timestamp whenHandedOver = model::Timestamp::now();
    medicationDispense.setWhenHandedOver(whenHandedOver);
    EXPECT_EQ(medicationDispense.whenHandedOver(), whenHandedOver);

    EXPECT_FALSE(medicationDispense.whenPrepared().has_value());
    const model::Timestamp whenPrepared = model::Timestamp::now();
    medicationDispense.setWhenPrepared(whenPrepared);
    EXPECT_TRUE(medicationDispense.whenPrepared().has_value());
    EXPECT_EQ(medicationDispense.whenPrepared().value(), whenPrepared);

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(medicationDispense.jsonDocument()),
        SchemaType::Gem_erxMedicationDispense));
}
