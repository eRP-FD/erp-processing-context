/*
* (C) Copyright IBM Deutschland GmbH 2022
* (C) Copyright IBM Corp. 2022
*/

#include "erp/ErpRequirements.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp11543Test : public ErpWorkflowTest
{
public:
    void SetUp() override
    {
        ErpWorkflowTest::SetUp();
        envVars = testutils::getNewFhirProfileEnvironment();
        std::string accessCode{};
        std::optional<model::KbvBundle> kbvBundle{};
        std::optional<model::ErxReceipt> closeReceipt{};

        mKvnr = generateNewRandomKVNR().id();
        ASSERT_NO_FATAL_FAILURE(
            createClosedTask(mPrescriptionId,
                             kbvBundle,
                             closeReceipt,
                             accessCode,
                             mSecret,
                             model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                             mKvnr));

        std::optional<model::Consent> consent{};
        ASSERT_NO_FATAL_FAILURE(consent = consentPost(mKvnr, model::Timestamp::now()));
        ASSERT_TRUE(consent.has_value());
    }

    void TearDown() override
    {
        envVars.clear();
        ErpWorkflowTest::TearDown();
    }

    void postChargeItem()
    {
        if (!mPostedChargeItem.has_value())
        {
            ASSERT_NO_FATAL_FAILURE(mPostedChargeItem = chargeItemPost(
                                        *mPrescriptionId,
                                        mKvnr,
                                        jwtApotheke().stringForClaim(JWT::idNumberClaim).value(),
                                        mSecret));
            ASSERT_TRUE(mPostedChargeItem);
            mPostedChargeItem->deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);

            mDispenseBundle = ResourceManager::instance().getStringResource(
                "test/EndpointHandlerTest/dispense_item.xml");
        }
    }

    std::optional<model::PrescriptionId> mPrescriptionId;
    std::optional<model::ChargeItem> mPostedChargeItem;
    std::optional<std::string> mDispenseBundle;
    std::string mKvnr;
    std::string mSecret;
private:
    std::vector<EnvironmentVariableGuard> envVars;
};

TEST_F(Erp11543Test, PostChargeItemWithBadKbvBundleSignature)
{
    A_22140.test("POST ChargeItem: bad signature for KBV dispense bundle should be rejected");

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

    postChargeItem();
    ASSERT_TRUE(mPostedChargeItem);
    ASSERT_TRUE(mDispenseBundle);

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

    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem{};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPut(
                                jwtApotheke(),
                                ContentMimeType::fhirXmlUtf8,
                                *mPostedChargeItem,
                                *mDispenseBundle,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid,
                                signFunction));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(chargeItem));
    const auto& operationOutcome = std::get<model::OperationOutcome>(chargeItem);
    ASSERT_TRUE(String::contains(operationOutcome.issues().at(0).detailsText.value_or(""),
                                 "CAdES-BES signature verification has failed"));
}

TEST_F(Erp11543Test, PutChargeItemWithBadKbvBundleSignatureCertificate)
{
    A_22151.test("PUT ChargeItem: bad certificate of KBV dispense bundle signature should be rejected");

    postChargeItem();
    ASSERT_TRUE(mPostedChargeItem);
    ASSERT_TRUE(mDispenseBundle);

    const auto signFunction = [](const std::string& data)
    {
        return CadesBesSignature{CryptoHelper::cQesG0(),
                                 CryptoHelper::cQesG0Prv(),
                                 data,
                                 std::nullopt}.getBase64();
    };

    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem;
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPut(
                                jwtApotheke(),
                                ContentMimeType::fhirXmlUtf8,
                                *mPostedChargeItem,
                                *mDispenseBundle,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid,
                                signFunction));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(chargeItem));
    const auto& operationOutcome = std::get<model::OperationOutcome>(chargeItem);
    ASSERT_TRUE(String::contains(operationOutcome.issues().at(0).detailsText.value_or(""), "Issuer is unknown"));
}
