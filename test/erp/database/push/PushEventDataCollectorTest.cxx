/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/model/PrescriptionId.hxx"

#include <gtest/gtest.h>

class PushEventDataCollectorTest : public testing::Test
{
};

TEST_F(PushEventDataCollectorTest, prescriptions)
{
    PushEventDataCollector collector;

    // Empty prescription id
    EXPECT_TRUE(collector.prescriptions().empty());
    EXPECT_FALSE(collector.prescriptionId());

    // Single prescription set:
    collector.setPrescriptionId(
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123));
    EXPECT_TRUE(collector.prescriptionId());
    EXPECT_EQ(collector.prescriptions().size(), 1);
    EXPECT_EQ(collector.prescriptionId().value().toString(), "160.000.000.000.123.76");
    EXPECT_EQ(collector.prescriptions()[0].toString(), "160.000.000.000.123.76");

    // Multiple prescriptions, overwrites prescriptionId() accessor:
    collector.setPrescriptions({
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 456),
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 457)});
    EXPECT_TRUE(collector.prescriptionId());
    EXPECT_EQ(collector.prescriptions().size(), 2);
    EXPECT_EQ(collector.prescriptionId().value().toString(), "160.000.000.000.456.47");
    EXPECT_EQ(collector.prescriptions()[0].toString(), "160.000.000.000.456.47");
    EXPECT_EQ(collector.prescriptions()[1].toString(), "160.000.000.000.457.44");

    // Single prescription set, resets prescriptions():
    collector.setPrescriptionId(
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123));
    EXPECT_TRUE(collector.prescriptionId());
    EXPECT_EQ(collector.prescriptions().size(), 1);
    EXPECT_EQ(collector.prescriptionId().value().toString(), "160.000.000.000.123.76");
    EXPECT_EQ(collector.prescriptions()[0].toString(), "160.000.000.000.123.76");
}

TEST_F(PushEventDataCollectorTest, readiness)
{
    PushEventDataCollector collector;
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.setPrescriptionId(
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123));
    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.setNotificationIdentifier("abcde");
    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.setHashedKvnr(db_model::HashedKvnr{1});
    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.setHashedKvnr(db_model::HashedKvnr{1});
    collector.setChannelId(model::ChannelId::unused);
    collector.validate();
    EXPECT_FALSE(collector.isReadyForPushEvent());

    collector.setHashedKvnr(db_model::HashedKvnr{1});
    collector.setChannelId(model::ChannelId::erp_chargeitem_create);
    collector.validate();
    EXPECT_TRUE(collector.isReadyForPushEvent());
}
