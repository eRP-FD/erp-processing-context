/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/Patient.hxx"

void model::KbvBundle::additionalValidation() const
{
    Resource::additionalValidation();
    const auto& medications = getResourcesByType<model::KbvMedicationGeneric>();
    for (const auto& medication : medications)
    {
        const auto profile = medication.getProfile();
        const auto* errorMessageFromAfo =
            "Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den vorgeschriebenen "
            "Prüfziffer-Validierungsregeln.";
        if (profile == SchemaType::KBV_PR_ERP_Medication_PZN)
        {
            const auto medicationPzn = model::KbvMedicationPzn::fromJson(medication.jsonDocument());
            A_23892.start("PZN valid checksum test");
            try
            {
                ErpExpect(medicationPzn.pzn().validChecksum(), HttpStatus::BadRequest, errorMessageFromAfo);
            }
            catch (const model::ModelException& ex)
            {
                ErpFailWithDiagnostics(HttpStatus::BadRequest, errorMessageFromAfo, ex.what());
            }
            A_23892.finish();
        }
        else if (profile == SchemaType::KBV_PR_ERP_Medication_Compounding)
        {
            const auto medicationCompounding = model::KbvMedicationCompounding::fromJson(medication.jsonDocument());
            A_24034.start("Validate PZN for ingredients");
            const auto& pzns = medicationCompounding.ingredientPzns();
            for (const auto& pzn : pzns)
            {
                ErpExpect(pzn.validChecksum(), HttpStatus::BadRequest, errorMessageFromAfo);
            }
            A_24034.finish();
        }
    }

    A_23936.start("Check for valid values of Patient.identifier.type.coding.code");
    const auto& patients = getResourcesByType<model::Patient>();
    for (const auto& patient : patients)
    {
        if (patient.hasIdentifier())
        {
            const auto& identifierTypeCodingCode = patient.identifierTypeCodingCode();
            ErpExpect(identifierTypeCodingCode == "GKV" || identifierTypeCodingCode == "PKV", HttpStatus::BadRequest,
                      "Als Identifier für den Patienten muss eine Versichertennummer angegeben werden.");
        }
    }
    A_23936.finish();

    const auto kbvCoverages = getResourcesByType<model::KbvCoverage>();
    for (const auto& kbvCoverage : kbvCoverages)
    {
        A_23888.start("Validate IK number");
        const auto payorIknr = kbvCoverage.payorIknr();
        ErpExpect(payorIknr.has_value() && payorIknr->validChecksum(), HttpStatus::BadRequest,
                  "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen im "
                  "Versicherungsstatus entspricht nicht den Prüfziffer-Validierungsregeln.");
        A_23888.finish();
        A_24030.start("Validate IK number of extension");
        const auto alternativeIknr = kbvCoverage.alternativeId();
        ErpExpect(! alternativeIknr.has_value() || alternativeIknr->validChecksum(), HttpStatus::BadRequest,
                  "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen des Kostenträgers "
                  "entspricht nicht den Prüfziffer-Validierungsregeln.");
        A_24030.finish();
    }
}

std::optional<model::Timestamp> model::KbvBundle::getValidationReferenceTimestamp() const
{
    const auto medicationRequests = getResourcesByType<model::KbvMedicationRequest>();
    ErpExpect(medicationRequests.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one MedicationRequest in Bundle.");
    return medicationRequests[0].authoredOn();
}
