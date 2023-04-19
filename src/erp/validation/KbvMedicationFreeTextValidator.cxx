/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvMedicationFreeTextValidator.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

void KbvMedicationFreeTextValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                              const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvMedicationFreeText&>(resource), xmlValidator, inCodeValidator);
}

void KbvMedicationFreeTextValidator_V1_0_2::doValidate(const model::KbvMedicationFreeText& kbvMedicationCompounding,
                                                       const XmlValidator&, const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category", kbvMedicationCompounding, true,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"}, {"00", "01", "02"});
    KbvValidationUtils::checkKbvExtensionValueBoolean(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine", kbvMedicationCompounding, true);
}
