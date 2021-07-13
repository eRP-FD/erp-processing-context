#include "erp/model/ErxReceipt.hxx"
#include "test_config.h"

#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/erp-serverinfo.hxx"

#include <gtest/gtest.h>


TEST(ErxReceiptTest, ConstructFromData)
{
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");

    const std::string telematicId = "12345654321";
    const model::Timestamp start = model::Timestamp::fromXsDateTime("2021-02-02T17:13:00+01:00");
    const model::Timestamp end = model::Timestamp::fromXsDateTime("2021-02-05T11:12:00+01:00");
    const std::string author = "https://prescriptionserver.telematik/Device/ErxService";
    const model::Composition composition(telematicId, start, end, author);

    const model::Device device;

    Uuid uuid;
    model::ErxReceipt erxReceipt(uuid, "ErxReceipt", prescriptionId, composition, author, device);

    EXPECT_EQ(erxReceipt.getId(), uuid);
    EXPECT_EQ(erxReceipt.prescriptionId().toDatabaseId(), prescriptionId.toDatabaseId());

    EXPECT_TRUE(erxReceipt.composition().telematicId().has_value());
    EXPECT_EQ(erxReceipt.composition().telematicId().value(), telematicId);
    EXPECT_TRUE(erxReceipt.composition().periodStart().has_value());
    EXPECT_EQ(erxReceipt.composition().periodStart().value(), start);
    EXPECT_TRUE(erxReceipt.composition().periodEnd().has_value());
    EXPECT_EQ(erxReceipt.composition().periodEnd().value(), end);
    EXPECT_TRUE(erxReceipt.composition().author().has_value());
    EXPECT_EQ(erxReceipt.composition().author().value(), author);

    EXPECT_EQ(erxReceipt.device().serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(erxReceipt.device().version(), ErpServerInfo::ReleaseVersion);

    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-02-10T09:45:11+01:00");
    const model::Signature origSignature(data, when, author);
    erxReceipt.setSignature(origSignature);
    const auto signature = erxReceipt.getSignature();
    ASSERT_TRUE(signature.has_value());
    EXPECT_TRUE(signature->when().has_value());
    EXPECT_EQ(signature->when().value(), when);
    EXPECT_TRUE(signature->data().has_value());
    EXPECT_EQ(signature->data().value(), data);
    EXPECT_TRUE(signature->who().has_value());
    EXPECT_EQ(signature->who().value(), author);
}


TEST(ErxReceiptTest, ConstructFromJson)
{
    const auto json = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/erx_receipt1.json");

    const model::ErxReceipt erxReceipt = model::ErxReceipt::fromJson(json);
    const auto prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58");

    const std::string telematicId = "606358757";
    const model::Timestamp start = model::Timestamp::fromXsDateTime("2020-03-20T07:23:34.328+00:00");
    const model::Timestamp end = model::Timestamp::fromXsDateTime("2020-03-20T12:21:34.558+00:00");
    const std::string author = "https://prescriptionserver.telematik/Device/ErxService";

    const std::string_view serialNumber = "R4.0.0.287342834";
    const std::string_view version = "1.0.0";

    EXPECT_EQ(erxReceipt.prescriptionId().toDatabaseId(), prescriptionId.toDatabaseId());

    EXPECT_TRUE(erxReceipt.composition().telematicId().has_value());
    EXPECT_EQ(erxReceipt.composition().telematicId().value(), telematicId);
    EXPECT_TRUE(erxReceipt.composition().periodStart().has_value());
    EXPECT_EQ(erxReceipt.composition().periodStart().value(), start);
    EXPECT_TRUE(erxReceipt.composition().periodEnd().has_value());
    EXPECT_EQ(erxReceipt.composition().periodEnd().value(), end);
    EXPECT_TRUE(erxReceipt.composition().author().has_value());
    EXPECT_EQ(erxReceipt.composition().author().value(), author);

    EXPECT_EQ(erxReceipt.device().serialNumber(), serialNumber);
    EXPECT_EQ(erxReceipt.device().version(), version);

    const auto signature = erxReceipt.getSignature();
    ASSERT_TRUE(signature.has_value());

    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-01-20T07:31:34.328+00:00");

    EXPECT_TRUE(signature->when().has_value());
    EXPECT_EQ(signature->when().value(), when);
    EXPECT_TRUE(signature->data().has_value());
    EXPECT_EQ(signature->data().value(), data);
    EXPECT_TRUE(signature->who().has_value());
    EXPECT_EQ(signature->who().value(), author);
}
