/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/Patient.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "test_config.h"

#include <gtest/gtest.h>

class BundleTest : public testing::Test
{
};


TEST_F(BundleTest, selfLink)
{
    model::Bundle bundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);
    bundle.setLink(model::Link::Type::Self, "this is the self link");

    ASSERT_TRUE(bundle.getLink(model::Link::Type::Self).has_value());
    ASSERT_EQ(bundle.getLink(model::Link::Type::Self).value(), "this is the self link");
}


TEST_F(BundleTest, id)
{
    const Uuid id;
    model::Bundle bundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile, id);

    ASSERT_EQ(bundle.getId(), id);
}


TEST_F(BundleTest, getResourceType)
{
    model::Bundle bundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);

    ASSERT_EQ(bundle.getResourceType(), "Bundle");
}


TEST_F(BundleTest, serializeToJsonString)
{
    model::Bundle bundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);
    bundle.setLink(model::Link::Type::Self, "self");

    const auto json = bundle.serializeToJsonString();

    ASSERT_TRUE(json.find("\"url\":\"self\"") != std::string::npos);
}


TEST_F(BundleTest, toJsonString)
{
    const auto bundle = model::Bundle::fromJsonNoValidation(R"(
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


TEST_F(BundleTest, getResourceSize)//NOLINT(readability-function-cognitive-complexity)
{
    model::Bundle bundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);

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

TEST_F(BundleTest, getResourcesByTypePatient)
{
    const auto bundleData = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/kbv_bundle1.json");
    const auto bundle = model::Bundle::fromJsonNoValidation(bundleData);

    auto patients = bundle.getResourcesByType<model::Patient>("Patient");
    ASSERT_EQ(patients.size(), 1);
    ASSERT_EQ((patients[0].kvnr()), "X234567891");
}

TEST_F(BundleTest, getSignatureWhen)
{
    const auto bundleData = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/kbv_bundle1.json");
    const auto bundle = model::Bundle::fromJsonNoValidation(bundleData);
    ASSERT_NO_THROW((void)bundle.getSignatureWhen());
}

TEST_F(BundleTest, searchSetTotal0)
{
    model::Bundle bundle(model::BundleType::searchset, ::model::FhirResourceBase::NoProfile);
    const auto& document = model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(bundle.jsonDocument());
    const rapidjson::Pointer totalPointer("/total");
    ASSERT_TRUE(totalPointer.Get(document));
    ASSERT_FALSE(totalPointer.Get(document)->IsNull());
    EXPECT_EQ(totalPointer.Get(document)->GetUint64(), 0);
}
