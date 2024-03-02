/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

class BdeV2Test : public ErpWorkflowTest
{
};

TEST_F(BdeV2Test, InvalidLanr)
{
    auto task = taskCreate();
    ASSERT_TRUE(task);
    const auto signingTime = model::Timestamp::now();
    model::Lanr defectLanr{"844444400", model::Lanr::Type::lanr};
    ASSERT_FALSE(defectLanr.validChecksum());
    auto kbvBundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = signingTime, .lanr = defectLanr});
    mActivateTaskRequestArgs.expectedLANR = defectLanr.id();
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), task->accessCode(),
                                         toCadesBesSignature(kbvBundle, signingTime), HttpStatus::OkWarnInvalidAnr));
}

TEST_F(BdeV2Test, InvalidZanr)
{
    auto task = taskCreate();
    ASSERT_TRUE(task);
    const auto signingTime = model::Timestamp::now();
    model::Lanr defectLanr{"123456789", model::Lanr::Type::zanr};
    ASSERT_FALSE(defectLanr.validChecksum());
    auto kbvBundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = signingTime, .lanr = defectLanr});
    mActivateTaskRequestArgs.expectedZANR = defectLanr.id();
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), task->accessCode(),
                                         toCadesBesSignature(kbvBundle, signingTime), HttpStatus::OkWarnInvalidAnr));
}
