/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/MedicationDispenseId.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ModelException.hxx"

#include <gtest/gtest.h>


TEST(PrescriptionIdTest, example1)
{
    // Example from
    // 2.2.1 Beispielrechnung
    // 2.2.1.1 Prüfzifferberechnung für "160.000.000.000.123.xx"
    A_19217.test("123 from DB is correctly translated into prescription notation");
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123);
    ASSERT_EQ(prescriptionId.toDatabaseId(), 123);
    ASSERT_EQ(prescriptionId.toString(), "160.000.000.000.123.76");
    A_19217.finish();

    A_19218.test("validate correct checksum 76");
    auto prescriptionId2 = model::PrescriptionId::fromString("160.000.000.000.123.76");
    A_19218.finish();
    ASSERT_EQ(prescriptionId2.toDatabaseId(), 123);
    ASSERT_EQ(prescriptionId2.toString(), "160.000.000.000.123.76");
}

TEST(PrescriptionIdTest, example2)
{
    // Example from
    // 2.2.1 Beispielrechnung
    // 2.2.1.3 Prüfzifferberechnung für "160.123.456.789.123.xx"
    A_19217.test("123456789123 from DB is correctly translated into prescription notation");
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123456789123);
    ASSERT_EQ(prescriptionId.toDatabaseId(), 123456789123);
    ASSERT_EQ(prescriptionId.toString(), "160.123.456.789.123.58");
    A_19217.finish();

    A_19218.test("validate correct checksum 58");
    auto prescriptionId2 = model::PrescriptionId::fromString("160.123.456.789.123.58");
    A_19218.finish();
    ASSERT_EQ(prescriptionId2.toDatabaseId(), 123456789123);
    ASSERT_EQ(prescriptionId2.toString(), "160.123.456.789.123.58");
}

TEST(PrescriptionIdTest, wrongChecksum)
{
    A_19218.test("validate incorrect checksums");
    ASSERT_ANY_THROW(model::PrescriptionId::fromString("160.123.456.789.123.11"));
    ASSERT_ANY_THROW(model::PrescriptionId::fromString("160.023.456.789.123.58"));
    ASSERT_ANY_THROW(model::PrescriptionId::fromString("161.123.456.789.123.58"));
}

TEST(PrescriptionIdTest, deriveUuid)
{
    auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto id2 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4712);
    Uuid uuid1(id.deriveUuid(1));
    Uuid uuid2(id.deriveUuid(std::numeric_limits<uint8_t>::max()));
    Uuid uuid3(id2.deriveUuid(1));
    Uuid uuid4(id2.deriveUuid(1));

    ASSERT_TRUE(uuid1.isValidIheUuid());
    ASSERT_TRUE(uuid2.isValidIheUuid());
    ASSERT_TRUE(uuid3.isValidIheUuid());
    ASSERT_TRUE(uuid4.isValidIheUuid());

    EXPECT_NE(uuid1, uuid2);
    EXPECT_NE(uuid1, uuid3);
    EXPECT_NE(uuid1, uuid4);
    EXPECT_NE(uuid2, uuid3);
    EXPECT_NE(uuid2, uuid4);
    EXPECT_EQ(uuid3, uuid4);
}

TEST(PrescriptionIdTest, deriveMedicationDispenseId)
{
    auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    using model::MedicationDispenseId;
    EXPECT_EQ(MedicationDispenseId(id, 0).toString(), id.toString());
    EXPECT_EQ(MedicationDispenseId(id, 1).toString(), id.toString() + "-1");
    EXPECT_EQ(MedicationDispenseId(id, 99).toString(), id.toString() + "-99");
}


TEST(PrescriptionIdTest, fromMedicationDispenseId)
{
    auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto str = id.toString();
    EXPECT_THROW((void)model::MedicationDispenseId::fromString(str + "-"), model::ModelException);
    EXPECT_THROW((void)model::MedicationDispenseId::fromString(str + "--"), model::ModelException);
    EXPECT_THROW((void)model::MedicationDispenseId::fromString(str + "-A"), model::ModelException);
    EXPECT_THROW((void)model::MedicationDispenseId::fromString(str + "1"), model::ModelException);
    EXPECT_THROW((void)model::MedicationDispenseId::fromString(str + "-0"), model::ModelException);

    {
        auto medid = model::MedicationDispenseId::fromString(str);
        EXPECT_EQ(medid.getPrescriptionId(), id);
        EXPECT_EQ(medid.getIndex(), 0);
    }
    {
        auto medid = model::MedicationDispenseId::fromString(str + "-1");
        EXPECT_EQ(medid.getPrescriptionId(), id);
        EXPECT_EQ(medid.getIndex(), 1);
    }
    {
        auto medid = model::MedicationDispenseId::fromString(str + "-99");
        EXPECT_EQ(medid.getPrescriptionId(), id);
        EXPECT_EQ(medid.getIndex(), 99);
    }
}

