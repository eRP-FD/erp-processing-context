/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <boost/algorithm/string/replace.hpp>

TEST_F(ErpWorkflowTest, AuthoredOnEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    auto timestamp = model::Timestamp::fromXsDateTime("2022-05-26T14:33:00+02:00");
    auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId.value(), .timestamp = timestamp});
    taskActivateWithOutcomeValidation(prescriptionId.value(), accessCode, toCadesBesSignature(bundle, timestamp), HttpStatus::OK);
}

TEST_F(ErpWorkflowTest, AuthoredOnNotEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    auto bundleTimestamp = model::Timestamp::fromXsDateTime("2022-05-26T14:33:00+02:00");
    auto signTimestamp = model::Timestamp::fromXsDateTime("2022-05-25T14:33:00+02:00");
    auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId.value(), .timestamp = bundleTimestamp});

    taskActivateWithOutcomeValidation(prescriptionId.value(), accessCode, toCadesBesSignature(bundle, signTimestamp), HttpStatus::BadRequest,
                 model::OperationOutcome::Issue::Type::invalid,
                 "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein");
}
