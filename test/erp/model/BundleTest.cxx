/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/Patient.hxx"

#include "test_config.h"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"

#include <gtest/gtest.h>

class BundleTest : public testing::Test
{
};


TEST_F(BundleTest, selfLink)
{
    model::Bundle bundle(model::Bundle::Type::collection);
    bundle.setLink(model::Link::Type::Self, "this is the self link");

    ASSERT_TRUE(bundle.getLink(model::Link::Type::Self).has_value());
    ASSERT_EQ(bundle.getLink(model::Link::Type::Self).value(), "this is the self link");
}


TEST_F(BundleTest, id)
{
    const Uuid id;
    model::Bundle bundle (model::Bundle::Type::collection, id);

    ASSERT_EQ(bundle.getId(), id);
}


TEST_F(BundleTest, getResourceType)
{
    model::Bundle bundle(model::Bundle::Type::collection);

    ASSERT_EQ(bundle.getResourceType(), "Bundle");
}


TEST_F(BundleTest, serrializeToJsonString)
{
    model::Bundle bundle(model::Bundle::Type::collection);
    bundle.setLink(model::Link::Type::Self, "self");

    const auto json = bundle.serializeToJsonString();

    ASSERT_TRUE(json.find("\"url\":\"self\"") != std::string::npos);
}


TEST_F(BundleTest, toJsonString)
{
    const auto bundle = model::Bundle::fromJson(R"(
    {
        "resourceType": "Bundle",
        "link": [
            {
                "relation": "self",
                "url":      "this is me"
            }
        ]
    }
    )");

    ASSERT_EQ(bundle.getResourceType(), "Bundle");
    ASSERT_EQ(bundle.getLink(model::Link::Type::Self), "this is me");
}


TEST_F(BundleTest, getResourceSize)
{
    model::Bundle bundle(model::Bundle::Type::collection);

    ASSERT_EQ(bundle.getResourceCount(), 0u);

    // Create a simple resource.
    rapidjson::StringStream s(R"(
    {
        "this": "is a resource"
    }
    )");
    model::NumberAsStringParserDocument resource;
    resource.ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
    ASSERT_FALSE(resource.HasParseError());

    // Add the resource to our bundle.
    bundle.addResource(
        {},
        "this is the resource's self link",
        {},
        resource);

    // Verify that it has been added.
    ASSERT_EQ(bundle.getResourceCount(), 1);

    // Verify the resource's link.
    const auto retrievedResourceLink = bundle.getResourceLink(0);
    ASSERT_EQ(retrievedResourceLink, "this is the resource's self link");

    // Verify the resource.
    const auto& retrievedResource = bundle.getResource(0);
    ASSERT_TRUE(retrievedResource.IsObject());
    ASSERT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&retrievedResource["this"]), "is a resource");
}

TEST_F(BundleTest, getTotalSearchMatches)
{
    model::Bundle bundle(model::Bundle::Type::searchset);

    ASSERT_EQ(bundle.getTotalSearchMatches(), 0);

    // Create a simple resource.
    rapidjson::StringStream s(R"(
    {
        "this": "is a resource"
    }
    )");
    model::NumberAsStringParserDocument resource;
    resource.ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
    ASSERT_FALSE(resource.HasParseError());

    bundle.addResource({}, "this is the resource's self link", model::Bundle::SearchMode::match, resource);
    bundle.addResource({}, "this is the resource's self link", model::Bundle::SearchMode::include, resource);
    bundle.addResource({}, "this is the resource's self link", model::Bundle::SearchMode::outcome, resource);
    bundle.addResource({}, "this is the resource's self link", model::Bundle::SearchMode::match, resource);

    ASSERT_EQ(bundle.getResourceCount(), 4);
    ASSERT_EQ(bundle.getTotalSearchMatches(), 2);
}

TEST_F(BundleTest, getResourcesByTypePatient)
{
    const auto bundleData = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/kbv_bundle1.json");
    const auto bundle = model::Bundle::fromJson(bundleData);

    auto patients = bundle.getResourcesByType<model::Patient>("Patient");
    ASSERT_EQ(patients.size(), 1);
    ASSERT_EQ((patients[0].getKvnr()), "X234567890");
}

TEST_F(BundleTest, getSignatureWhen)
{
    const auto bundleData = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/kbv_bundle1.json");
    const auto bundle = model::Bundle::fromJson(bundleData);
    ASSERT_NO_THROW((void)bundle.getSignatureWhen());
}

TEST_F(BundleTest, searchSetTotal0)
{
    model::Bundle bundle(model::Bundle::Type::searchset);
    const auto& document = model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(bundle.jsonDocument());
    const rapidjson::Pointer totalPointer("/total");
    ASSERT_TRUE(totalPointer.Get(document));
    ASSERT_FALSE(totalPointer.Get(document)->IsNull());
    EXPECT_EQ(totalPointer.Get(document)->GetUint64(), 0);
}
