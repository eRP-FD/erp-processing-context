/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Signature.hxx"

#include <gtest/gtest.h>


TEST(SignatureTest, ConstructFromData)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-02-10T09:45:11+01:00");
    const std::string whoReference = "https://prescriptionserver.telematik/Device/ErxService";
    const std::string whoDisplay = "ERP Fachdienst";
    const model::Signature signature(data, when, whoReference, whoDisplay);

    EXPECT_TRUE(signature.when().has_value());
    EXPECT_EQ(signature.when().value(), when);
    EXPECT_TRUE(signature.data().has_value());
    EXPECT_EQ(signature.data().value(), data);
    EXPECT_TRUE(signature.whoReference().has_value());
    EXPECT_EQ(signature.whoReference().value(), whoReference);
    EXPECT_TRUE(signature.whoDisplay().has_value());
    EXPECT_EQ(signature.whoDisplay().value(), whoDisplay);
}


TEST(SignatureTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string json = R"(
{
    "resourceType":"Signature",
    "type":[
        {
            "system":"urn:iso-astm:E1762-95:2013",
            "code":"1.2.840.10065.1.12.1.1",
            "display":"Author's Signature"
        }
    ],
    "when":"2021-01-20T07:31:34.328+00:00",
    "who":{
        "reference":"https://prescriptionserver.telematik/Device/ErxService",
        "display":"ERP Fachdienst"
    },
    "targetFormat":"application/fhir+json",
    "sigFormat":"application/pkcs7-mime",
    "data":"QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2"
})";

    const auto signature = model::Signature::fromJsonNoValidation(json);

    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-01-20T07:31:34.328+00:00");
    const std::string whoReference = "https://prescriptionserver.telematik/Device/ErxService";
    const std::string whoDisplay = "ERP Fachdienst";

    EXPECT_TRUE(signature.when().has_value());
    EXPECT_EQ(signature.when().value(), when);
    EXPECT_TRUE(signature.data().has_value());
    EXPECT_EQ(signature.data().value(), data);
    EXPECT_TRUE(signature.whoReference().has_value());
    EXPECT_EQ(signature.whoReference().value(), whoReference);
    EXPECT_TRUE(signature.whoDisplay().has_value());
    EXPECT_EQ(signature.whoDisplay().value(), whoDisplay);
}
