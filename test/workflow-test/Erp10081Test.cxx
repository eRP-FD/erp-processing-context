/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp10081Test : public ErpWorkflowTest
{
};

TEST_F(Erp10081Test, apothekenpflichtigeArzneimittelPkv)
{
    const std::string kvnr = generateNewRandomKVNR();
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret,
                         model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, kvnr));

    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, model::Timestamp::now()));
    ASSERT_TRUE(consent.has_value());

    const auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> chargeItem;
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(*prescriptionId, kvnr, telematikId, secret));
    ASSERT_TRUE(chargeItem);
}

TEST_F(Erp10081Test, apothekenpflichtigeArzneimittel)
{
    const std::string kvnr = generateNewRandomKVNR();
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret,
                         model::PrescriptionType::apothekenpflichigeArzneimittel, kvnr));

    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, model::Timestamp::now()));
    ASSERT_TRUE(consent.has_value());

    const auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> chargeItem;

    A_22731.test("wrong flow type for ChargeItem");
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(
                                *prescriptionId,
                                kvnr,
                                telematikId,
                                secret,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid));
    ASSERT_FALSE(chargeItem);
}
