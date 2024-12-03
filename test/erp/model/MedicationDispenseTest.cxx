/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Resource.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>



TEST(MedicationDispenseTest, WrongSchemaMissingIdentifier)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyIdentifiers)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":"https://gematik.de/fhir/NamingSystem/Covid-19",
            "value":"456"
        },
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier1)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
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
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier2)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
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
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongIdentifier3)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
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
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingSubject)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
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
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongSubject)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
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
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingPerformer)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaTooManyPerformers)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
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
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaWrongPerformer)
{
    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":"http://fhir.de/NamingSystem/gkv/kvid-10",
                    "value":"X123456788"
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}

TEST(MedicationDispenseTest, WrongSchemaMissingWhenHandedOver)
{

    using namespace std::string_literals;
    const std::string json = R"(
{
    "resourceType":"MedicationDispense",
    "id": "160.000.000.004.715.74",
    "meta":{
        "profile":[
            ")"s.append(model::resource::structure_definition::medicationDispense) +
                             '|' + to_string(ResourceTemplates::Versions::GEM_ERP_current()) + R"("
        ]
    },
    "identifier":[
        {
            "system":")" + std::string{model::resource::naming_system::prescriptionID} +
                             R"(",
            "value":"160.000.000.004.715.74"
        }
    ],
    "status":"completed",
    "medicationReference":{
        "reference":"Medication/1234"
    },
    "subject":{
        "identifier":{
            "system": ")" + std::string{model::resource::naming_system::gkvKvid10} +
                             R"(",
            "value":"X123456788"
        }
    },
    "performer":[
        {
            "actor":{
                "identifier":{
                    "system":")" +
                             std::string{model::resource::naming_system::telematicID} + R"(",
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

    EXPECT_THROW((void) model::MedicationDispense::fromJson(json, *StaticData::getJsonValidator()), ErpException);
}


struct MedicationDispenseDateTimeTestParam {
    ResourceTemplates::Versions::GEM_ERP version;
    std::string whenHandedOver;
    std::string whenPrepared;
    bool expectOK;
};

class MedicationDispenseDateTimeTest : public testing::TestWithParam<MedicationDispenseDateTimeTestParam>
{
};

TEST_P(MedicationDispenseDateTimeTest, whenHandedOverWhenPreparedDateTime)
{
    using namespace std::string_view_literals;
    testutils::ShiftFhirResourceViewsGuard unshift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    using namespace std::string_literals;
    auto medicatioDispenseXml = ResourceTemplates::medicationDispenseXml({
        .whenHandedOver = GetParam().whenHandedOver,
        .whenPrepared = GetParam().whenPrepared,
        .gematikVersion = GetParam().version,
    });

    if (GetParam().expectOK)
    {
        EXPECT_NO_THROW((void) model::MedicationDispense::fromXml(medicatioDispenseXml, *StaticData::getXmlValidator()));
    }
    else
    {
        try {
            (void) model::MedicationDispense::fromXml(medicatioDispenseXml, *StaticData::getXmlValidator());
            FAIL();
        }
        catch (const ErpException& ex)
        {
            ASSERT_TRUE(ex.diagnostics().has_value());
            const auto& diag = *ex.diagnostics();
            ASSERT_FALSE(std::ranges::search(diag, "workflow-abgabeDatumsFormat"sv).empty()) << diag;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(valid, MedicationDispenseDateTimeTest,
                         ::testing::ValuesIn(std::list<MedicationDispenseDateTimeTestParam>{
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01T00:00:00+01:00",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2025-03-01T00:00:00+01:00",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_3,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2024-11-01T00:00:00+01:00",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_3,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = true,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_4,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = true,
                             },
                         }));

INSTANTIATE_TEST_SUITE_P(invalidDateTime, MedicationDispenseDateTimeTest,
                         ::testing::ValuesIn(std::list<MedicationDispenseDateTimeTestParam>{
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_3,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01T00:00:00+01:00",
                                 .expectOK = false,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_3,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = false,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_4,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01T00:00:00+01:00",
                                 .expectOK = false,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_4,
                                 .whenHandedOver = "2025-03-02",
                                 .whenPrepared = "2025-03-01T00:00:00+01:00",
                                 .expectOK = false,
                             },
                             {
                                 .version = ResourceTemplates::Versions::GEM_ERP_1_4,
                                 .whenHandedOver = "2025-03-02T00:00:00+01:00",
                                 .whenPrepared = "2025-03-01",
                                 .expectOK = false,
                             },
                         }));
