/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/EvdgaBundle.hxx"
#include "erp/model/EvdgaHealthAppRequest.hxx"
#include "KbvCoverage.hxx"
#include "shared/ErpRequirements.hxx"

namespace model
{

std::optional<Timestamp> EvdgaBundle::getValidationReferenceTimestamp() const
{
    return authoredOn();
}

Timestamp EvdgaBundle::authoredOn() const
{
    const auto deviceRequests = getResourcesByType<model::EvdgaHealthAppRequest>();
    ErpExpect(deviceRequests.size() == 1, HttpStatus::BadRequest,
              "Für diesen Workflowtypen sind nur Verordnungen für Digitale Gesundheitsanwendungen zulässig");
    return deviceRequests[0].authoredOn();
}

void EvdgaBundle::additionalValidation() const
{
    try
    {
        A_25991.start("DeviceRequest-Ressource");
        // Covered by FHIR-Profile
        A_25991.finish();
        A_25991.start("e16D");// Covered by FHIR-Profile
        A_25991.finish();
        A_26372.start("no Coverage.payor.identifier.extension:alternativeID allowed");
        auto coverage = getResourcesByType<KbvCoverage>();
        ErpExpect(coverage.size() == 1, HttpStatus::BadRequest, "Expected exactly one Coverage resource");
        ErpExpect(
            ! coverage[0].hasPayorIdentifierExtension(resource::extension::alternativeIk), HttpStatus::BadRequest,
            "Workflow nicht für Verordnungen nutzbar, die zu Lasten von Unfallkassen oder Berufsgenossenschaften gehen");
        A_26372.finish();
        A_25992.start("PZN-checksum in KBV_PR_EVDGA_HealthAppRequest.codeCodeableConcept.coding.code");
        auto evdgaHealthAppRequests = getResourcesByType<EvdgaHealthAppRequest>();
        ErpExpect(evdgaHealthAppRequests.size() == 1, HttpStatus::BadRequest,
                  "Expected exactly one DeviceRequest resource");
        ErpExpect(evdgaHealthAppRequests[0].getPzn().validChecksum(), HttpStatus::BadRequest,
                  "Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den vorgeschriebenen "
                  "Prüfziffer-Validierungsregeln.");
        A_25992.finish();
    }
    catch (const ModelException& me)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "exception during additional validation", me.what());
    }
}

};
