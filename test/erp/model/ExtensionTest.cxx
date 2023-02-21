/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"

#include <gtest/gtest.h>

#include "test/util/ResourceTemplates.hxx"


class MyTestExtension : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "MyTestExtension";

    friend std::optional<MyTestExtension> ResourceBase::getExtension<MyTestExtension>(const std::string_view&) const;
};

TEST(ExtensionTest, extensionWithValue)
{
    static constexpr auto extension = R"(
    {
        "url": "MyTestExtension",
        "valueBoolean": true
    })";

    auto ext = model::Extension::fromJsonNoValidation(extension);
    std::optional<bool> valueBoolean;
    ASSERT_NO_THROW(valueBoolean = ext.valueBoolean());
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_TRUE(*valueBoolean);
}

TEST(ExtensionTest, extensionNoValue)
{
    static constexpr auto extension = R"(
    {
        "url": "MyTestExtension"
    })";

    auto ext = model::Extension::fromJsonNoValidation(extension);
    std::optional<bool> valueBoolean;
    ASSERT_NO_THROW(valueBoolean = ext.valueBoolean());
    ASSERT_FALSE(valueBoolean.has_value());
}

TEST(ExtensionTest, extensionFound)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document",
        "extension": [
            {
                "url": "MyTestExtension",
                "valueBoolean": true
            }
        ]

    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_TRUE(ext.has_value());
    auto valueBoolean = ext->valueBoolean();
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_TRUE(*valueBoolean);
}

TEST(ExtensionTest, noExtension)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document"
    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_FALSE(ext.has_value());
}


TEST(ExtensionTest, NoSuchExtension)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document",
        "extension": [
            {
                "url": "SomeOtherExtension",
                "valueBoolean": true
            }
        ]

    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_FALSE(ext.has_value());
}


TEST(ExtensionTest, multipleExtensionsFirst)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document",
        "extension": [
            {
                "url": "MyTestExtension",
                "valueBoolean": false
            },
            {
                "url": "SomeOtherExtension"
            }
        ]

    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_TRUE(ext.has_value());
    auto valueBoolean = ext->valueBoolean();
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_FALSE(*valueBoolean);
}


TEST(ExtensionTest, multipleExtensionsLast)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document",
        "extension": [
            {
                "url": "SomeOtherExtension",
                "valueBoolean": false
            },
            {
                "url": "MyTestExtension",
                "valueBoolean": true
            }
        ]

    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_TRUE(ext.has_value());
    auto valueBoolean = ext->valueBoolean();
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_TRUE(*valueBoolean);
}

TEST(ExtensionTest, multipleExtensionsMiddle)
{
    static constexpr auto minBundle = R"(
    {
        "resourceType": "Bundle",
        "type": "document",
        "extension": [
            {
                "url": "SomeOtherExtension1",
                "valueBoolean": false
            },
            {
                "url": "MyTestExtension",
                "valueBoolean": true
            },
            {
                "url": "SomeOtherExtension2"
            }
        ]

    })";

    auto bundle = model::Bundle::fromJsonNoValidation(minBundle);
    auto ext = bundle.getExtension<MyTestExtension>();
    ASSERT_TRUE(ext.has_value());
    auto valueBoolean = ext->valueBoolean();
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_TRUE(*valueBoolean);
}

TEST(ExtensionTest, kbvBundle)
{
    auto bundle = model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml());
    auto medicationRequest = bundle.getResourcesByType<model::KbvMedicationRequest>();
    ASSERT_EQ(medicationRequest.size(), 1);
    auto ext = medicationRequest.at(0).getExtension<model::KBVMultiplePrescription>();
    ASSERT_TRUE(ext.has_value());
    auto kennzeichen = ext->getExtension<model::KBVMultiplePrescription::Kennzeichen>();
    ASSERT_TRUE(kennzeichen.has_value());
    auto valueBoolean = kennzeichen->valueBoolean();
    ASSERT_TRUE(valueBoolean.has_value());
    ASSERT_FALSE(*valueBoolean);
}
