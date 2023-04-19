/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvMedicationRequestValidator.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/extensions/KBVEXERPAccident.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "erp/validation/KbvValidationUtils.hxx"


void KbvMedicationRequestValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                             const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvMedicationRequest&>(resource), xmlValidator, inCodeValidator);
}

void KbvMedicationRequestValidator_V1_0_2::doValidate(const model::KbvMedicationRequest& kbvMedicationRequest,
                                                      const XmlValidator&, const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_StatusCoPayment", kbvMedicationRequest, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_StatusCoPayment"}, {"0", "1", "2"});
    KbvValidationUtils::checkKbvExtensionValueBoolean(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_EmergencyServicesFee", kbvMedicationRequest, true);
    KbvValidationUtils::checkKbvExtensionValueBoolean("https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_BVG",
                                                      kbvMedicationRequest, true);

    const auto& accidentExtension = kbvMedicationRequest.getExtension<model::KBVEXERPAccident>();
    if (accidentExtension.has_value())
    {
        KbvValidationUtils::checkKbvExtensionValueCoding("unfallkennzeichen", *accidentExtension, true,
                                                         {"https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"},
                                                         {"1", "2", "4"});
        KbvValidationUtils::checkKbvExtensionValueString("unfallbetrieb", *accidentExtension, false);
        KbvValidationUtils::checkKbvExtensionValueDate("unfalltag", *accidentExtension, false);
    }

    const auto& multiplePrescriptionExtension = kbvMedicationRequest.getExtension<model::KBVMultiplePrescription>();
    if (multiplePrescriptionExtension.has_value())
    {
        KbvValidationUtils::checkKbvExtensionValueBoolean("Kennzeichen", *multiplePrescriptionExtension, true);
        KbvValidationUtils::checkKbvExtensionValueRatio("Nummerierung", *multiplePrescriptionExtension, false);
        KbvValidationUtils::checkKbvExtensionValuePeriod("Zeitraum", *multiplePrescriptionExtension, false, true);
    }

    const auto& dosageInstruction = kbvMedicationRequest.dosageInstruction();
    if (dosageInstruction.has_value())
    {
        KbvValidationUtils::checkKbvExtensionValueBoolean(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag", *dosageInstruction, false);

        erp_angabeDosierung(*dosageInstruction);
    }
}

void KbvMedicationRequestValidator_V1_0_2::erp_angabeDosierung(const model::Dosage& dosage) const
{
    // -erp-angabeDosierung
    // (extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag').empty()
    //      or extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag').value.as(Boolean)=false)
    // implies text.empty()
    static const auto *const constraintMessage =
        "-erp-angabeDosierung: Wenn das Dosierungskennzeichen nicht gesetzt ist, darf auch kein Text vorhanden sein.";
    const auto& dosageFlagExtension = KbvValidationUtils::checkGetExtension(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag", dosage, false);
    if ((! dosageFlagExtension || ! dosageFlagExtension->valueBoolean().value()))
    {
        ErpExpect(!dosage.text(), HttpStatus::BadRequest, KbvValidationUtils::constraintError(constraintMessage));
    }
}
