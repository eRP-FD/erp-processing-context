/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Binary.hxx"

#include <gtest/gtest.h>


TEST(BinaryTest, Create)
{
    const std::string id = "binary_id";
    const std::string data = "binary_data_content";
    model::Binary binary(id, data);
    ASSERT_EQ(id, binary.id());
    ASSERT_EQ(data, binary.data());
}


TEST(BinaryTest, CreateFromJson)
{
    const std::string binaryJson = R"--(
        {
            "resourceType": "Binary",
            "id": "my-Id",
            "meta": {
                "versionId": "1",
                "profile": ["https://gematik.de/fhir/StructureDefinition/ErxBinary"]
            },
            "contentType": "application/pkcs7-mime",
            "data" : "my-Data"
        })--";
    model::Binary binary = model::Binary::fromJson(binaryJson);
    ASSERT_EQ(binary.id(), "my-Id");
    ASSERT_EQ(binary.data(), "my-Data");
}
