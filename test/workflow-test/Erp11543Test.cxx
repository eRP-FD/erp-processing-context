/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/ErpRequirements.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp11543Test : public ErpWorkflowTest
{
public:
    void SetUp() override
    {
        ErpWorkflowTest::SetUp();
        std::string accessCode{};

        mKvnr = generateNewRandomKVNR().id();
        ASSERT_NO_FATAL_FAILURE(
            createClosedTask(mPrescriptionId,
                             mKbvBundle,
                             mCloseReceipt,
                             accessCode,
                             mSecret,
                             model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                             mKvnr));
        mCloseReceipt->removeSignature();

        std::optional<model::Consent> consent{};
        ASSERT_NO_FATAL_FAILURE(consent = consentPost(mKvnr, model::Timestamp::now()));
        ASSERT_TRUE(consent.has_value());
    }

    void TearDown() override
    {
        envVars.clear();
        ErpWorkflowTest::TearDown();
    }

    void createAndGetChargeItem()
    {
        if (!mCreatedChargeItem.has_value())
        {
            ASSERT_NO_FATAL_FAILURE(mCreatedChargeItem = chargeItemPost(
                                        *mPrescriptionId,
                                        mKvnr,
                                        jwtApotheke().stringForClaim(JWT::idNumberClaim).value(),
                                        mSecret));
            ASSERT_TRUE(mCreatedChargeItem);

            const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(mKvnr);
            std::optional<model::Bundle> chargeItemsBundle;
            ASSERT_NO_FATAL_FAILURE(
                chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                    *mPrescriptionId, std::nullopt,
                                                    mKbvBundle, mCloseReceipt));
            auto chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
            EXPECT_EQ(chargeItems.size(), 1);
            mCreatedChargeItem = model::ChargeItem::fromJsonNoValidation(chargeItems[0].serializeToJsonString());
            mCreatedChargeItem->deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);

            mDispenseItem = ResourceTemplates::davDispenseItemXml({.prescriptionId = *mPrescriptionId});
        }
    }

    std::optional<model::PrescriptionId> mPrescriptionId;
    std::optional<model::ChargeItem> mCreatedChargeItem;
    std::optional<std::string> mDispenseItem;
    std::string mKvnr;
    std::string mSecret;
    std::optional<model::KbvBundle> mKbvBundle;
    std::optional<model::ErxReceipt> mCloseReceipt;

private:
    std::vector<EnvironmentVariableGuard> envVars;
};

TEST_F(Erp11543Test, PostChargeItemWithBadKbvBundleSignature)
{
    A_22140_01.test("POST ChargeItem: bad signature for KBV dispense bundle should be rejected");

    const auto signFunction = [](const std::string& data)
    {
        CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(),
                                            CryptoHelper::cHpQesPrv(),
                                            data,
                                            std::nullopt};

        auto badSignature = Base64::decode(cadesBesSignature.getBase64());
        if (!badSignature.empty())
        {
            badSignature.front()++;
        }

        return Base64::encode(badSignature);
    };

    std::optional<model::ChargeItem> chargeItem{};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(
                                *mPrescriptionId,
                                mKvnr,
                                jwtApotheke().stringForClaim(JWT::idNumberClaim).value(),
                                mSecret,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid,
                                std::nullopt,
                                signFunction));
    ASSERT_FALSE(chargeItem);
}

TEST_F(Erp11543Test, PostChargeItemWithBadKbvBundleSignatureCertificate)
{
    A_22141.test("POST ChargeItem: bad certificate of KBV dispense bundle signature should be rejected");

    const auto signFunction = [](const std::string& data)
    {
        return CadesBesSignature{CryptoHelper::cQesG0(),
                                 CryptoHelper::cQesG0Prv(),
                                 data,
                                 std::nullopt}.getBase64();
    };

    std::optional<model::ChargeItem> chargeItem{};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(
                                *mPrescriptionId,
                                mKvnr,
                                jwtApotheke().stringForClaim(JWT::idNumberClaim).value(),
                                mSecret,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid,
                                std::nullopt,
                                signFunction));
    ASSERT_FALSE(chargeItem);
}

TEST_F(Erp11543Test, PutChargeItemWithBadKbvBundleSignature)
{
    A_22150.test("PUT ChargeItem: bad signature for KBV dispense bundle should be rejected");

    ASSERT_NO_FATAL_FAILURE(createAndGetChargeItem());
    ASSERT_TRUE(mCreatedChargeItem);
    ASSERT_TRUE(mDispenseItem);

    const auto signFunction = [](const std::string& data)
    {
        CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(),
                                            CryptoHelper::cHpQesPrv(),
                                            data,
                                            std::nullopt};

        auto badSignature = Base64::decode(cadesBesSignature.getBase64());
        if (!badSignature.empty())
        {
            badSignature.front()++;
        }

        return Base64::encode(badSignature);
    };

    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem{model::OperationOutcome(model::OperationOutcome::Issue{})};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *mCreatedChargeItem,
                                                       *mDispenseItem, mCreatedChargeItem->accessCode().value(),
                                                       HttpStatus::BadRequest,
                                                       model::OperationOutcome::Issue::Type::invalid, signFunction));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(chargeItem));
    const auto& operationOutcome = std::get<model::OperationOutcome>(chargeItem);
    ASSERT_TRUE(String::contains(operationOutcome.issues().at(0).detailsText.value_or(""),
                                 "CAdES-BES signature verification has failed"));
}

TEST_F(Erp11543Test, PutChargeItemWithBadKbvBundleSignatureCertificate)
{
    A_22151_01.test("PUT ChargeItem: bad certificate of KBV dispense bundle signature should be rejected");

    ASSERT_NO_FATAL_FAILURE(createAndGetChargeItem());
    ASSERT_TRUE(mCreatedChargeItem);
    ASSERT_TRUE(mDispenseItem);

    const auto signFunction = [](const std::string& data)
    {
        return CadesBesSignature{CryptoHelper::cQesG0(),
                                 CryptoHelper::cQesG0Prv(),
                                 data,
                                 std::nullopt}.getBase64();
    };

    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem{model::OperationOutcome(model::OperationOutcome::Issue{})};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *mCreatedChargeItem,
                                                       *mDispenseItem, mCreatedChargeItem->accessCode().value(),
                                                       HttpStatus::BadRequest,
                                                       model::OperationOutcome::Issue::Type::invalid, signFunction));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(chargeItem));
    const auto& operationOutcome = std::get<model::OperationOutcome>(chargeItem);
    ASSERT_TRUE(String::contains(operationOutcome.issues().at(0).detailsText.value_or(""), "Issuer is unknown"));
}
