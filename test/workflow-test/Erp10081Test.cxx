/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp10081Test : public ErpWorkflowTestP
{
};

TEST_P(Erp10081Test, ChargeItemFlowType)
{
    const std::string kvnr = generateNewRandomKVNR().id();
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr));

    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, model::Timestamp::now()));
    ASSERT_TRUE(consent.has_value());

    const auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> chargeItem;

    switch(GetParam())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::direkteZuweisung:
            A_22731.test("wrong flow type for ChargeItem");
            mChargeItemRequestArgs.overrideExpectedDavVersion = "XXX";
            mChargeItemRequestArgs.overrideExpectedPatientenrechnungVersion = "XXX";
            ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(
                                        *prescriptionId,
                                        kvnr,
                                        telematikId,
                                        secret,
                                        HttpStatus::BadRequest,
                                        model::OperationOutcome::Issue::Type::invalid));
            ASSERT_FALSE(chargeItem);
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(*prescriptionId, kvnr, telematikId, secret));
            ASSERT_TRUE(chargeItem);
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(Erp10081TestInst, Erp10081Test,
                         testing::ValuesIn(magic_enum::enum_values<model::PrescriptionType>()));
