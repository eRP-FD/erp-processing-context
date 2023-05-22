/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "KbvBundle.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/KbvMedicationPzn.hxx"

void model::KbvBundle::additionalValidation() const
{
    Resource::additionalValidation();
    const auto& medications = getResourcesByType<model::KbvMedicationGeneric>();
    for (const auto& medication : medications)
    {
        const auto profile = medication.getProfile();
        if (profile == SchemaType::KBV_PR_ERP_Medication_PZN)
        {
            auto medicationPzn = model::KbvMedicationPzn::fromJson(medication.jsonDocument());
            A_22925.start("PZN length constraint check");
            const auto* errorMessageFromAfo = "Länge PZN unzulässig (muss 8-stellig sein)";
            try
            {
                const auto pznString = medicationPzn.pzn();
                ErpExpect(pznString.size() == 8, HttpStatus::BadRequest, errorMessageFromAfo);
            }
            catch (const model::ModelException& ex)
            {
                ErpFailWithDiagnostics(HttpStatus::BadRequest, errorMessageFromAfo, ex.what());
            }
            A_22925.finish();
        }
    }
}
