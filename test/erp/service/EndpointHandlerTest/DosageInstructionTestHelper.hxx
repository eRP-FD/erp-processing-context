/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "EndpointHandlerTestFixture.hxx"
#include "shared/model/MedicationDispenseOperationParameters.hxx"

#include <rapidjson/pointer.h>
#include <vector>


namespace model
{
class NumberAsStringParserDocument;
}
class DosageInstructionTestHelper
{
public:
    static model::NumberAsStringParserDocument patchSample(model::NumberAsStringParserDocument&& sample,
                                                           const model::NumberAsStringParserDocument& input,
                                                           const rapidjson::Pointer& extensionArrayPointer,
                                                           const rapidjson::Pointer& dosageInstructionPtr);

    static std::vector<std::string> makeTestParams(const std::string& dir, const std::string& filter);
    static std::string paramToString(const testing::TestParamInfo<std::string>& info);
    static model::MedicationDispenseOperationParameters getDispenseSample(model::PrescriptionId prescriptionId,
                                                                          const std::string& sampleFile,
                                                                          model::ProfileType profileType);
    static model::NumberAsStringParserDocument
    patchMedicationDispense(model::NumberAsStringParserDocument&& parameters,
                            const model::NumberAsStringParserDocument& medicationDispense);
};
