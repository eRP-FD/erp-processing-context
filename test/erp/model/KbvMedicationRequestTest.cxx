/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/DosageDgMP.hxx"
#include "shared/model/KbvBundle.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>

class KbvMedicationRequestTest : public ::testing::Test {};

TEST_F(KbvMedicationRequestTest, MovePatientInstructionToText)
{
    auto kbvBundle = model::KbvBundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml({}));
    auto medicationRequest = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    const rapidjson::Pointer patientInstructionPointer("/dosageInstruction/0/patientInstruction");
    auto doc = std::move(medicationRequest).jsonDocument();
    doc.setValue(patientInstructionPointer, "hello world");
    medicationRequest = model::KbvMedicationRequest::fromJson(std::move(doc));
    medicationRequest.movePatientInstructionToText();
    EXPECT_EQ(medicationRequest.dosageInstruction().at(0).text(), "hello world");
    EXPECT_EQ(patientInstructionPointer.Get(medicationRequest.jsonDocument()), nullptr);
}

