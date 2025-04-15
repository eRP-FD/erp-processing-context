/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Bundle.hxx"
#include "shared/model/Extension.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>


class MyTestExtension : public model::UnspecifiedExtension
{
public:
    using UnspecifiedExtension::UnspecifiedExtension;
    MyTestExtension(UnspecifiedExtension&& unspec)
        : UnspecifiedExtension{std::move(unspec)}
    {
    }
    MyTestExtension& operator=(UnspecifiedExtension&& unspec)
    {
        UnspecifiedExtension::operator=(std::move(unspec));
        return *this;
    }
    static constexpr auto url = "MyTestExtension";
};

TEST(ExtensionTest, extensionWithValue)
{
    static constexpr auto extension = R"(
    {
        "url": "MyTestExtension",
        "valueBoolean": true
    })";

    auto ext = model::UnspecifiedExtension::fromJsonNoValidation(extension);
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

    auto ext = model::UnspecifiedExtension::fromJsonNoValidation(extension);
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
