/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ErxReceipt.hxx"
#include "test_config.h"

#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/erp-serverinfo.hxx"

#include <gtest/gtest.h>
#include <test/util/EnvironmentVariableGuard.hxx>


TEST(ErxReceiptTest, ConstructFromData)//NOLINT(readability-function-cognitive-complexity)
{
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.715.74");

    const std::string telematicId = "12345654321";
    const model::Timestamp start = model::Timestamp::fromXsDateTime("2021-02-02T17:13:00+01:00");
    const model::Timestamp end = model::Timestamp::fromXsDateTime("2021-02-05T11:12:00+01:00");
    const std::string author = "https://prescriptionserver.telematik/Device/ErxService";
    const std::string_view prescriptionDigestIdentifier = "Binary/PrescriptionDigest-160.000.000.004.715.74";


    for (const auto& profileVersion : {::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1})
    {
        EnvironmentVariableGuard forceGematikVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION",
                                                     ::std::string{::model::ResourceVersion::v_str(profileVersion)}};

        const model::Composition composition(telematicId, start, end, author, prescriptionDigestIdentifier);

        const model::Device device;
        const ::model::Binary prescriptionDigestResource{"TestDigest", "Test", ::model::Binary::Type::Digest};
        Uuid uuid;

        model::ErxReceipt erxReceipt(uuid, "ErxReceipt", prescriptionId, composition, author, device, "TestDigest",
                                     prescriptionDigestResource);

        EXPECT_EQ(erxReceipt.getId(), uuid);
        EXPECT_EQ(erxReceipt.prescriptionId().toDatabaseId(), prescriptionId.toDatabaseId());

        EXPECT_TRUE(erxReceipt.composition().telematikId().has_value());
        EXPECT_EQ(erxReceipt.composition().telematikId().value(), telematicId);
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
        const auto digest = erxReceipt.prescriptionDigest();

        EXPECT_EQ(digest.jsonDocument().getOptionalStringValue(rapidjson::Pointer{"/contentType"}),
                  "application/octet-stream");
        EXPECT_EQ(digest.data().value(), "Test");
        EXPECT_TRUE(erxReceipt.composition().prescriptionDigestIdentifier().has_value());
        EXPECT_EQ(erxReceipt.composition().prescriptionDigestIdentifier().value(), prescriptionDigestIdentifier);
    }
}


TEST(ErxReceiptTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    for (const auto& profileVersion : {::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1})
    {
        EnvironmentVariableGuard forceGematikVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION",
                                                     ::std::string{::model::ResourceVersion::v_str(profileVersion)}};

        const auto json =
            FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/erx_receipt1_1_1.json");

        const model::ErxReceipt erxReceipt = model::ErxReceipt::fromJsonNoValidation(json);
        const auto prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58");

        const std::string telematicId = "606358757";
        const model::Timestamp start = model::Timestamp::fromXsDateTime("2020-03-20T07:23:34.328+00:00");
        const model::Timestamp end = model::Timestamp::fromXsDateTime("2020-03-20T12:21:34.558+00:00");
        const std::string author = "https://prescriptionserver.telematik/Device/ErxService";

        const std::string_view serialNumber = "R4.0.0.287342834";
        const std::string_view version = "1.4.0";

        EXPECT_EQ(erxReceipt.prescriptionId().toDatabaseId(), prescriptionId.toDatabaseId());

        EXPECT_TRUE(erxReceipt.composition().telematikId().has_value());
        EXPECT_EQ(erxReceipt.composition().telematikId().value(), telematicId);
        EXPECT_TRUE(erxReceipt.composition().periodStart().has_value());
        EXPECT_EQ(erxReceipt.composition().periodStart().value(), start);
        EXPECT_TRUE(erxReceipt.composition().periodEnd().has_value());
        EXPECT_EQ(erxReceipt.composition().periodEnd().value(), end);
        EXPECT_TRUE(erxReceipt.composition().author().has_value());
        EXPECT_EQ(erxReceipt.composition().author().value(), author);

        EXPECT_EQ(erxReceipt.device().serialNumber(), serialNumber);
        EXPECT_EQ(erxReceipt.device().version(), version);

        const auto digest = erxReceipt.prescriptionDigest();

        EXPECT_EQ(digest.jsonDocument().getOptionalStringValue(rapidjson::Pointer{"/contentType"}),
                  "application/octet-stream");
        EXPECT_EQ(digest.data().value(), "Test");
        EXPECT_TRUE(erxReceipt.composition().prescriptionDigestIdentifier().has_value());
        EXPECT_EQ(erxReceipt.composition().prescriptionDigestIdentifier().value(), "Binary/TestDigest");
    }
}
