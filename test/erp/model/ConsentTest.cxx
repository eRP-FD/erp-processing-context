/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceManager.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/String.hxx"

#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

class ConsentTest : public testing::Test
{
};


TEST_F(ConsentTest, ConsentId)//NOLINT(readability-function-cognitive-complexity)
{
    const model::Kvnr kvnr{"X987654326"};
    const auto consentType = model::Consent::Type::CHARGCONS;
    const auto consentIdStr = std::string("CHARGCONS-") + kvnr.id();

    EXPECT_EQ(model::Consent::createIdString(consentType, kvnr), consentIdStr);

    auto consentIdParts = model::Consent::splitIdString(consentIdStr);
    EXPECT_EQ(model::Consent::createIdString(consentIdParts.first, model::Kvnr{consentIdParts.second}), consentIdStr);
    EXPECT_EQ(consentIdParts.first, consentType);
    EXPECT_EQ(consentIdParts.second, kvnr);

    EXPECT_THROW(consentIdParts = model::Consent::splitIdString(""), model::ModelException);
    EXPECT_THROW(consentIdParts = model::Consent::splitIdString("-X123456788"), model::ModelException);
    EXPECT_THROW(consentIdParts = model::Consent::splitIdString("INVALID-X123456788"), model::ModelException);
}

TEST_F(ConsentTest, Construct)
{
    if (!model::ResourceVersion::supportedBundles()
            .contains(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01))
    {
        // constructor of BaseResource uses versionizeProfile, which fails when 2023-07-01 is not supported.
        GTEST_SKIP();
    }
    const auto consentType = model::Consent::Type::CHARGCONS;
    const model::Kvnr kvnr{"X123456788"};
    const model::Timestamp dateTime = model::Timestamp::now();
    model::Consent consent(kvnr, dateTime);
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id(), model::Consent::createIdString(consentType, kvnr));
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), dateTime.toXsDateTimeWithoutFractionalSeconds());
    EXPECT_TRUE(consent.isChargingConsent());
}

TEST_F(ConsentTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    auto jsonString = ResourceManager::instance().getStringResource("test/EndpointHandlerTest/consent_input.json");

    std::optional<model::Consent> optConsent;
    ASSERT_NO_THROW(optConsent = model::Consent::fromJson(
        jsonString, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(), SchemaType::Gem_erxConsent,
            {model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01}));

    auto& consent = optConsent.value();

    EXPECT_FALSE(consent.id().has_value());
    const model::Kvnr kvnr{"X123456788"};
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    EXPECT_TRUE(consent.isChargingConsent());
    EXPECT_NO_THROW(consent.fillId());
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id().value(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));

    // Set category/coding to some other values to check that now "isChargingConsent" is false:
    jsonString = String::replaceAll(
        jsonString, "https://gematik.de/fhir/erpchrg/CodeSystem/GEM_ERPCHRG_CS_ConsentType", "http://terminology.hl7.org/CodeSystem/v3-ActCode");
    jsonString = String::replaceAll(jsonString, "CHARGCONS", "INFAO");

    std::optional<model::Consent> optConsent2;
    ASSERT_NO_THROW(optConsent2 = model::Consent::fromJsonNoValidation(jsonString));
    EXPECT_FALSE(optConsent2.value().isChargingConsent());
    EXPECT_THROW(optConsent2.value().fillId(), model::ModelException);
}

TEST_F(ConsentTest, ConstructFromXml)//NOLINT(readability-function-cognitive-complexity)
{
    auto consent = model::Consent::fromXml(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/consent_input.xml"),
        *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::Gem_erxConsent,
                                    {model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01});

    EXPECT_FALSE(consent.id().has_value());
    const model::Kvnr kvnr{"X123456788"};
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    EXPECT_TRUE(consent.isChargingConsent());
    EXPECT_NO_THROW(consent.fillId());
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id().value(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
}
