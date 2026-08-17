/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/PushNotificationEvent.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"
class MedicationExporterDatabaseTest : public PostgresDatabaseTest
{
};

TEST_F(MedicationExporterDatabaseTest, processNextPushNotification)
{
    model::Kvnr kvnr("X678901238");
    std::optional<model::PushNotificationEvent> pn;
    ASSERT_NO_THROW(pn = database().processNextPushNotification());
    database().commitTransaction();
    ASSERT_FALSE(pn);
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 999'999'999'999);
    insertPushEvent(kvnr, prescriptionId, "erp.task.activate", Uuid{});
    ASSERT_NO_THROW(pn = database().processNextPushNotification());
    database().commitTransaction();
    ASSERT_TRUE(pn);
    EXPECT_EQ(pn->hashedKvnr().toHex(), mKeyDerivation->hashKvnr(kvnr).toHex());
    EXPECT_EQ(pn->retryCount(), 0);
    EXPECT_EQ(pn->pushNotification().channelId(), "erp.task.activate");
    EXPECT_EQ(pn->pushNotification().identifier(), prescriptionId.toString());
    EXPECT_EQ(pn->pushNotification().identifierType(), "TaskId");

    ASSERT_NO_THROW(pn = database().processNextPushNotification());
    database().commitTransaction();
    ASSERT_FALSE(pn);
}

TEST_F(MedicationExporterDatabaseTest, updatePushProcessingDelay)
{
    model::Kvnr kvnr("X678901238");
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    auto eventId = insertPushEvent(kvnr, prescriptionId, "erp.task.activate", Uuid{});
    database().commitTransaction();
    ASSERT_NO_THROW(
        database().updatePushProcessingDelay(3, std::chrono::seconds{10}, mKeyDerivation->hashKvnr(kvnr), eventId));
    database().commitTransaction();
    auto now = model::Timestamp::now();

    std::optional<model::PushNotificationEvent> pn;
    ASSERT_NO_THROW(pn = database().processNextPushNotification());
    database().commitTransaction();
    ASSERT_FALSE(pn);

    testutils::waitFor(
        [&] {
            return now + std::chrono::seconds{10} < model::Timestamp::now();
        },
        10s);

    ASSERT_NO_THROW(pn = database().processNextPushNotification());
    database().commitTransaction();
    ASSERT_TRUE(pn);

    EXPECT_EQ(pn->hashedKvnr().toHex(), mKeyDerivation->hashKvnr(kvnr).toHex());
    EXPECT_EQ(pn->retryCount(), 3);
    EXPECT_EQ(pn->pushNotification().channelId(), "erp.task.activate");
    EXPECT_EQ(pn->pushNotification().identifier(), prescriptionId.toString());
    EXPECT_EQ(pn->pushNotification().identifierType(), "TaskId");
}