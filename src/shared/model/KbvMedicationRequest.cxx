/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/KbvMedicationRequest.hxx"
#include "DosageDgMP.hxx"
#include "extensions/GeneratedDosageInstructionsMeta.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/util/dosagetext/Validator.hxx"

#include <date/tz.h>

namespace model
{

KbvMedicationRequest::KbvMedicationRequest(NumberAsStringParserDocument&& document)
    : Resource<KbvMedicationRequest>(std::move(document))
{
}

std::optional<std::string_view> KbvMedicationRequest::statusCoPaymentExtension() const
{
    static const rapidjson::Pointer extensionArrayPointer("/extension");
    static const rapidjson::Pointer urlPointer("/url");
    static const rapidjson::Pointer codePointer("/valueCoding/code");
    return findStringInArray(extensionArrayPointer, urlPointer, resource::structure_definition::kbvExErpStatusCoPayment,
                             codePointer);
}

bool model::KbvMedicationRequest::isMultiplePrescription() const
{
    auto multiplePrescription = getExtension<KBVMultiplePrescription>();
    bool res = multiplePrescription.has_value() && multiplePrescription->isMultiplePrescription();
    return res;
}

std::optional<date::year_month_day> KbvMedicationRequest::mvoEndDate() const
{
    auto mPExt = getExtension<model::KBVMultiplePrescription>();
    if (mPExt)
    {
        if (mPExt->isMultiplePrescription())
        {
            auto endDate = mPExt->endDate();
            if (endDate.has_value())
            {
                return date::year_month_day{endDate->localDay(Timestamp::GermanTimezone)};
            }
        }
    }
    return std::nullopt;
}

std::vector<DosageDgMP> KbvMedicationRequest::dosageInstruction() const
{
    return getDosageInstructionFromResource(*this);
}

model::Timestamp KbvMedicationRequest::authoredOn() const
{
    static const rapidjson::Pointer authoredOnPointer(resource::ElementName::path("authoredOn"));
    const auto authoredOnStr = getStringValue(authoredOnPointer);
    return model::Timestamp::fromGermanDate(std::string{authoredOnStr});
}

std::optional<model::Timestamp> KbvMedicationRequest::getValidationReferenceTimestamp() const
{
    return authoredOn();
}

void KbvMedicationRequest::additionalValidation() const
{
    if (getProfileVersionChecked() < version::KBV_1_4)
    {
        return;
    }
    A_28567_01.start("Anwendung der Validierung wenn dosageInstruction oder renderedDosageInstruction vorhanden ist");
    A_28570.start("Task aktivieren - Prüfung strukturierte Dosierung");
    const auto& dosage = dosageInstruction();
    const auto& renderedDosageExtension =
        getExtension(resource::structure_definition::extension_MedicationRequest_renderedDosageInstruction);
    if (! dosage.empty() || renderedDosageExtension.has_value())
    {
        ErpExpect(renderedDosageExtension.has_value(), HttpStatus::BadRequest,
                  "Validation of rendered dosage-instructions: Missing "
                  "MedicationRequest.MedicationRequest_renderedDosageInstruction extension.");
        ErpExpect(! dosage.empty(), HttpStatus::BadRequest,
                  "Validation of rendered dosage-instructions: Missing MedicationRequest.dosageInstruction.");
        const auto& metaExtension = getExtension<GeneratedDosageInstructionsMeta>();
        ErpExpect(metaExtension.has_value(), HttpStatus::BadRequest,
                  "Validation of rendered dosage-instructions: Missing "
                  "MedicationRequest.GeneratedDosageInstructionsMeta extension.");
        dosagetext::Validator::validate(dosage, *renderedDosageExtension, *metaExtension);
    }
}

void KbvMedicationRequest::movePatientInstructionToText()
{
    static const rapidjson::Pointer dosageInstructionPointer(resource::ElementName::path("dosageInstruction"));
    static const rapidjson::Pointer patientInstructionPointer(resource::ElementName::path("patientInstruction"));
    static const rapidjson::Pointer textPointer(resource::ElementName::path("text"));
    auto* dosageInstructionArray = getValue(dosageInstructionPointer);
    if (dosageInstructionArray != nullptr && dosageInstructionArray->IsArray())
    {
        for (auto& dosageInstruction : dosageInstructionArray->GetArray())
        {
            if (const auto* patientInstruction = patientInstructionPointer.Get(dosageInstruction))
            {
                setKeyValue(dosageInstruction, textPointer, NumberAsStringParserDocument::getStringValueFromValue(patientInstruction));
            }
            patientInstructionPointer.Erase(dosageInstruction);
        }
    }
}

}
