/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/service/EndpointHandlerTest/DosageInstructionTestHelper.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <set>

model::NumberAsStringParserDocument DosageInstructionTestHelper::patchSample(
    model::NumberAsStringParserDocument&& sample, const model::NumberAsStringParserDocument& input,
    const rapidjson::Pointer& extensionArrayPointer, const rapidjson::Pointer& dosageInstructionPtr)
{
    const rapidjson::Pointer extensionPointer("/extension");
    const rapidjson::Pointer dosageInstructionPointer("/dosageInstruction");
    sample.setKeyValue(sample, dosageInstructionPtr, *dosageInstructionPointer.Get(input));
    std::set<size_t> remove;

    for (size_t i = 0; const auto& extension : extensionArrayPointer.Get(sample)->GetArray())
    {
        const std::string url{
            model::NumberAsStringParserDocument::valueAsString(*rapidjson::Pointer("/url").Get(extension))};
        if (url == model::resource::structure_definition::extension_MedicationDispense_renderedDosageInstruction ||
            url == model::resource::structure_definition::extension_MedicationRequest_renderedDosageInstruction ||
            url == model::resource::structure_definition::GeneratedDosageInstructionsMeta)
        {
            remove.insert(i);
        }
        ++i;
    }
    for (const size_t i : remove | std::views::reverse)
    {
        sample.removeFromArray(extensionArrayPointer, i);
    }
    for (const auto& extension : extensionPointer.Get(input)->GetArray())
    {
        sample.copyToArray(extensionArrayPointer, extension);
    }
    return std::move(sample);
}

std::vector<std::string> DosageInstructionTestHelper::makeTestParams(const std::string& dir, const std::string& filter)
{
    const std::filesystem::directory_iterator it{ResourceManager::instance().getAbsoluteFilename("test/dosage/" + dir)};
    std::vector<std::string> filenames;
    for (const auto& entry : it)
    {
        if (entry.path().filename().string().find(filter) != std::string::npos)
        {
            filenames.emplace_back((dir / entry.path().filename()).string());
        }
    }
    std::ranges::sort(filenames);
    return filenames;
}
std::string DosageInstructionTestHelper::paramToString(const testing::TestParamInfo<std::string>& info)
{
    return std::regex_replace(info.param, std::regex("[^A-Za-z0-9_]"), "_");
}
model::MedicationDispenseOperationParameters
DosageInstructionTestHelper::getDispenseSample(model::PrescriptionId prescriptionId, const std::string& sampleFile,
                                               model::ProfileType profileType)
{
    {
        ResourceTemplates::MedicationDispenseOperationParametersOptions options{
            .profileType = profileType};
        options.medicationDispenses.front().prescriptionId = prescriptionId.toString();
        auto sample = model::MedicationDispenseOperationParameters::fromXmlNoValidation(
            ResourceTemplates::medicationDispenseOperationParametersXml(options));
        const auto filename = "test/dosage/" + sampleFile;
        const auto resourceJson = ResourceManager::instance().getStringResource(filename);
        const auto resource = model::UnspecifiedResource::fromJsonNoValidation(resourceJson);
        return model::MedicationDispenseOperationParameters::fromJson(
            patchMedicationDispense(std::move(sample).jsonDocument(), resource.jsonDocument()));
    }
}
model::NumberAsStringParserDocument
DosageInstructionTestHelper::patchMedicationDispense(model::NumberAsStringParserDocument&& parameters,
                                                     const model::NumberAsStringParserDocument& medicationDispense)
{
    const rapidjson::Pointer medicationDispenseExtensionPointer("/parameter/0/part/0/resource/extension");
    const rapidjson::Pointer medicationDispenseDosageInstructionPointer(
        "/parameter/0/part/0/resource/dosageInstruction");
    std::cout << parameters.serializeToJsonString() << std::endl;
    return patchSample(std::move(parameters), medicationDispense, medicationDispenseExtensionPointer,
                       medicationDispenseDosageInstructionPointer);
}
