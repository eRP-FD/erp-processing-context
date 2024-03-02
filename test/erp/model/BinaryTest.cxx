/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Binary.hxx"
#include "erp/util/String.hxx"
#include "erp/model/ResourceNames.hxx"

#include <gtest/gtest.h>

namespace model
{

class FriendlyBinary : public Binary
{
    using Binary::Binary;

    FRIEND_TEST(BinaryTest, Constructor);
};

TEST(BinaryTest, Constructor)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string id = "binary_id";
    const std::string data = "binary_data_content";
    const auto profilePointer = ::rapidjson::Pointer{"/meta/profile/0"};
    const auto metaVersionIdPointer = ::rapidjson::Pointer{"/meta/versionId"};
    const auto contentTypePointer = ::rapidjson::Pointer{"/contentType"};
    const auto profileVersion = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    bool isDeprecated = deprecatedProfile(profileVersion);
    auto sdBinary = isDeprecated ? model::resource::structure_definition::deprecated::binary
                                 : model::resource::structure_definition::binary;

    auto sdDigest = isDeprecated ? model::resource::structure_definition::deprecated::digest
                                 : model::resource::structure_definition::digest;
    {
        model::FriendlyBinary binary(id, data, Binary::Type::PKCS7);
        ASSERT_EQ(id, binary.id());
        ASSERT_EQ(data, binary.data());

        const auto profile = binary.getOptionalStringValue(profilePointer);
        ASSERT_TRUE(profile);
        const auto profileParts = String::split(profile.value(), '|');
        ASSERT_GE(profileParts.size(), 1);

        EXPECT_EQ(profileParts[0], sdBinary);

        const auto contentType = binary.getOptionalStringValue(contentTypePointer);
        ASSERT_TRUE(contentType.has_value());
        EXPECT_EQ(contentType.value(), "application/pkcs7-mime");
        const auto metaVersionId = binary.getOptionalStringValue(metaVersionIdPointer);
        ASSERT_TRUE(metaVersionId.has_value());
        EXPECT_EQ(metaVersionId, "1");
    }

    {
        model::FriendlyBinary binary(id, data, ::model::Binary::Type::Digest, profileVersion, "SomeMetaVersionId");
        ASSERT_EQ(id, binary.id());
        ASSERT_EQ(data, binary.data());

        const auto profile = binary.getOptionalStringValue(profilePointer);
        ASSERT_TRUE(profile);
        const auto profileParts = String::split(profile.value(), '|');
        ASSERT_GE(profileParts.size(), 1);
        EXPECT_EQ(profileParts[0], sdDigest);

        const auto contentType = binary.getOptionalStringValue(contentTypePointer);
        ASSERT_TRUE(contentType.has_value());
        EXPECT_EQ(contentType.value(), "application/octet-stream");
        const auto metaVersionId = binary.getOptionalStringValue(metaVersionIdPointer);
        ASSERT_TRUE(metaVersionId.has_value());
        EXPECT_EQ(metaVersionId, "SomeMetaVersionId");
    }

    {
        model::FriendlyBinary binary(id, data, Binary::Type::PKCS7, profileVersion, std::nullopt);
        EXPECT_FALSE(binary.getOptionalStringValue(metaVersionIdPointer).has_value());
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
