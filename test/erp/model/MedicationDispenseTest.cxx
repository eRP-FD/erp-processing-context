/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>



TEST(MedicationDispenseTest, WrongSchemaMissingIdentifier)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(
                     json, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                     *StaticData::getInCodeValidator(), SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyIdentifiers)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/Covid-19",
            "value":"456"
        },
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(
                     json, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                     *StaticData::getInCodeValidator(), SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier1)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
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
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier2)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
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
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier3)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
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
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingSubject)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
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
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongSubject)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
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
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingPerformer)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyPerformers)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
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
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongPerformer)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingWhenHandedOver)
{

    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator(),
                                                            *StaticData::getXmlValidator(),
                                                            *StaticData::getInCodeValidator(),
                                                            SchemaType::Gem_erxMedicationDispense),
                 ErpException);
}

TEST(MedicationDispenseTest, CorrectSchema)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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
    std::optional<model::MedicationDispense> medicationDispense1;
    EXPECT_NO_THROW(medicationDispense1 = model::MedicationDispense::fromJson(
                        json, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                        *StaticData::getInCodeValidator(), SchemaType::Gem_erxMedicationDispense));
    auto& medicationDispense = *medicationDispense1;

    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");
    medicationDispense.setPrescriptionId(prescriptionId);
    medicationDispense.setId({prescriptionId, 0});
    EXPECT_EQ(medicationDispense.prescriptionId().toString(), prescriptionId.toString());

    const model::Kvnr kvnr{"X424242424"};
    medicationDispense.setKvnr(kvnr);
    EXPECT_EQ(medicationDispense.kvnr(), kvnr);

    const model::TelematikId telematicId{"1234567"};
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
}

TEST(MedicationDispenseTest, ERP_6610_optionalTime)//NOLINT(readability-function-cognitive-complexity)
{
    static constexpr auto copyToOriginalFormat = &model::NumberAsStringParserDocumentConverter::copyToOriginalFormat;
    using model::resource::ElementName;
    namespace elements = model::resource::elements;

    static const rapidjson::Pointer whenHandedOver(ElementName::path(elements::whenHandedOver));
    static const rapidjson::Pointer whenPrepared(ElementName::path(elements::whenPrepared));

    const std::string jsonStr = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")" + testutils::profile(SchemaType::Gem_erxMedicationDispense) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{testutils::prescriptionIdNamespace()} + R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{testutils::gkvKvid10()} + R"(",
            "value":"X123456789"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" + std::string{testutils::telematikIdNamespace()}  + R"(",
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
    auto md = model::NumberAsStringParserDocument::fromJson(jsonStr);
    auto validate = [&] {
        StaticData::getJsonValidator()->validate(copyToOriginalFormat(md), SchemaType::fhir);
        auto xmlStr = Fhir::instance().converter().jsonToXmlString(md);
        (void) model::MedicationDispense::fromXml(xmlStr, *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(),
                                                  SchemaType::Gem_erxMedicationDispense);
    };


    whenHandedOver.Set(md, md.makeString("2020-03-20T07:13:00+05:00"));
    EXPECT_NO_THROW(validate());

    whenHandedOver.Set(md, md.makeString("2020-03-20"));
    EXPECT_NO_THROW(validate());


    whenPrepared.Set(md, md.makeString("2020-03-19T07:13:00+05:00"));
    EXPECT_NO_THROW(validate());
whenPrepared.Set(md, md.makeString("2020-03-20"));
    EXPECT_NO_THROW(validate());
}
