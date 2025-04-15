/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

class Erp10941Test : public ErpWorkflowTest
{
};

TEST_F(Erp10941Test, PostChargeItemByPharmacyWithMarkingFlagFails)
{
    const std::string kvnr = generateNewRandomKVNR().id();
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
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(
                                *prescriptionId,
                                kvnr,
                                telematikId,
                                secret,
                                HttpStatus::BadRequest,
                                model::OperationOutcome::Issue::Type::invalid,
                                "test/EndpointHandlerTest/charge_item_ERP10941_template.xml"));
    ASSERT_FALSE(chargeItem);
}
