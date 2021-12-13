/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Signature.hxx"

#include <gtest/gtest.h>


TEST(SignatureTest, ConstructFromData)
{
    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-02-10T09:45:11+01:00");
    const std::string who = "https://prescriptionserver.telematik/Device/ErxService";
    const model::Signature signature(data, when, who);

    EXPECT_TRUE(signature.when().has_value());
    EXPECT_EQ(signature.when().value(), when);
    EXPECT_TRUE(signature.data().has_value());
    EXPECT_EQ(signature.data().value(), data);
    EXPECT_TRUE(signature.who().has_value());
    EXPECT_EQ(signature.who().value(), who);
}


TEST(SignatureTest, ConstructFromJson)
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
        "reference":"https://prescriptionserver.telematik/Device/ErxService"
    },
    "targetFormat":"application/fhir+json",
    "sigFormat":"application/pkcs7-mime",
    "data":"QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2"
})";

    const auto signature = model::Signature::fromJsonNoValidation(json);

    const std::string data = "QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2";
    const model::Timestamp when = model::Timestamp::fromXsDateTime("2021-01-20T07:31:34.328+00:00");
    const std::string who = "https://prescriptionserver.telematik/Device/ErxService";

    EXPECT_TRUE(signature.when().has_value());
    EXPECT_EQ(signature.when().value(), when);
    EXPECT_TRUE(signature.data().has_value());
    EXPECT_EQ(signature.data().value(), data);
    EXPECT_TRUE(signature.who().has_value());
    EXPECT_EQ(signature.who().value(), who);
}
