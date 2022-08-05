/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ResourceManager.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/String.hxx"

#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


TEST(ConsentTest, ConsentId)//NOLINT(readability-function-cognitive-complexity)
{
    const char* const kvnr = "X987654321";
    const auto consentType = model::Consent::Type::CHARGCONS;
    const auto consentIdStr = std::string("CHARGCONS-") + kvnr;

    EXPECT_EQ(model::Consent::createIdString(consentType, kvnr), consentIdStr);

    auto consentIdParts = model::Consent::splitIdString(consentIdStr);
    EXPECT_EQ(model::Consent::createIdString(consentIdParts.first, consentIdParts.second), consentIdStr);
    EXPECT_EQ(consentIdParts.first, consentType);
    EXPECT_EQ(consentIdParts.second, kvnr);

    EXPECT_THROW(consentIdParts = model::Consent::splitIdString(""), model::ModelException);
    EXPECT_THROW(consentIdParts = model::Consent::splitIdString("-X123456789"), model::ModelException);
    EXPECT_THROW(consentIdParts = model::Consent::splitIdString("INVALID-X123456789"), model::ModelException);
}

TEST(ConsentTest, Construct)
{
    const auto consentType = model::Consent::Type::CHARGCONS;
    const char* const kvnr = "X123456789";
    const fhirtools::Timestamp dateTime = fhirtools::Timestamp::now();
    model::Consent consent(kvnr, dateTime);
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id(), model::Consent::createIdString(consentType, kvnr));
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), dateTime.toXsDateTimeWithoutFractionalSeconds());
    EXPECT_TRUE(consent.isChargingConsent());
}

TEST(ConsentTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard validationModeGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE", "disable"};
    auto jsonString = ResourceManager::instance().getStringResource("test/EndpointHandlerTest/consent_input.json");

    std::optional<model::Consent> optConsent;
    ASSERT_NO_THROW(optConsent = model::Consent::fromJson(
        jsonString, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(), SchemaType::fhir)); // TODO change to correct schema as soon as available

    auto& consent = optConsent.value();

    EXPECT_FALSE(consent.id().has_value());
    const char* const kvnr = "X123456789";
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    EXPECT_TRUE(consent.isChargingConsent());
    EXPECT_NO_THROW(consent.fillId());
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id().value(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));

    // Set category/coding to some other values to check that now "isChargingConsent" is false:
    jsonString = String::replaceAll(
        jsonString, "https://gematik.de/fhir/CodeSystem/Consenttype", "http://terminology.hl7.org/CodeSystem/v3-ActCode");
    jsonString = String::replaceAll(jsonString, "CHARGCONS", "INFAO");

    std::optional<model::Consent> optConsent2;
    ASSERT_NO_THROW(optConsent2 = model::Consent::fromJson(
        jsonString, *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
        *StaticData::getInCodeValidator(), SchemaType::fhir));
    EXPECT_FALSE(optConsent2.value().isChargingConsent());
    EXPECT_THROW(optConsent2.value().fillId(), model::ModelException);
}

TEST(ConsentTest, ConstructFromXml)//NOLINT(readability-function-cognitive-complexity)
{
    auto consent = model::Consent::fromXml(
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/consent_input.xml"),
        *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::fhir);  // TODO change to correct schema as soon as available

    EXPECT_FALSE(consent.id().has_value());
    const char* const kvnr = "X123456789";
    EXPECT_EQ(consent.patientKvnr(), kvnr);
    EXPECT_EQ(consent.dateTime().toXsDateTimeWithoutFractionalSeconds(), "2021-06-01T02:13:00+00:00");

    EXPECT_TRUE(consent.isChargingConsent());
    EXPECT_NO_THROW(consent.fillId());
    EXPECT_TRUE(consent.id().has_value());
    EXPECT_EQ(consent.id().value(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
}

