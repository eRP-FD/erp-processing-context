/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Binary.hxx"
#include "erp/util/String.hxx"

#include <gtest/gtest.h>

namespace model
{

class FriendlyBinary : public Binary
{
    using Binary::Binary;

    FRIEND_TEST(BinaryTest, Constructor);
};

TEST(BinaryTest, Constructor)
{
    const std::string id = "binary_id";
    const std::string data = "binary_data_content";
    const auto profilePointer = ::rapidjson::Pointer{"/meta/profile/0"};
    const auto contentTypePointer = ::rapidjson::Pointer{"/contentType"};

    {
        model::FriendlyBinary binary(id, data);
        ASSERT_EQ(id, binary.id());
        ASSERT_EQ(data, binary.data());

        const auto profile = binary.getOptionalStringValue(profilePointer);
        ASSERT_TRUE(profile);
        const auto profileParts = ::String::split(profile.value(), '|');
        ASSERT_GE(profileParts.size(), 1);
        EXPECT_EQ(profileParts[0], "https://gematik.de/fhir/StructureDefinition/ErxBinary");

        const auto contentType = binary.getOptionalStringValue(contentTypePointer);
        ASSERT_TRUE(contentType.has_value());
        EXPECT_EQ(contentType.value(), "application/pkcs7-mime");
    }

    {
        model::FriendlyBinary binary(id, data, ::model::Binary::Type::Base64);
        ASSERT_EQ(id, binary.id());
        ASSERT_EQ(data, binary.data());

        const auto profile = binary.getOptionalStringValue(profilePointer);
        ASSERT_TRUE(profile);
        const auto profileParts = ::String::split(profile.value(), '|');
        ASSERT_GE(profileParts.size(), 1);
        EXPECT_EQ(profileParts[0], "http://hl7.org/fhir/StructureDefinition/Binary");

        const auto contentType = binary.getOptionalStringValue(contentTypePointer);
        ASSERT_TRUE(contentType.has_value());
        EXPECT_EQ(contentType.value(), "application/octet-stream");
    }
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
    model::Binary binary = model::Binary::fromJsonNoValidation(binaryJson);
    ASSERT_EQ(binary.id(), "my-Id");
    ASSERT_EQ(binary.data(), "my-Data");
}

}
