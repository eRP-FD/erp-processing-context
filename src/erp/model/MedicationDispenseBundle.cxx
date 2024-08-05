/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{

/**
 * @see ERP-22297
 * @brief
 * If the request body does not have a meta profile, it is set to MedicationDispenseBundle by default.
 * This method can be removed once the acceptance of unqualified packets has ceased.
 */
void MedicationDispenseBundle::prepare()
{
    Resource::prepare();
    if (!getProfileName())
    {
        const auto medications = getResourcesByType<model::MedicationDispense>();
        ErpExpect(! medications.empty(), HttpStatus::BadRequest,
                "Expected at least one MedicationDispense in MedicationDispenseBundle");

        const auto profileName = medications[0].getProfileName();
        ModelExpect(profileName.has_value(), "missing meta.profile");
        fhirtools::DefinitionKey key{*profileName};
        ModelExpect(key.version.has_value(), "missing version on meta.profile: " + to_string(key));

        const auto referenceTimestamp = getValidationReferenceTimestamp();
        ModelExpect(referenceTimestamp.has_value(), "missing reference timestamp.");
        const auto& fhirInstance = Fhir::instance();
        const auto viewList = fhirInstance.structureRepository(*referenceTimestamp);
        ModelExpect(! viewList.empty(), "Invalid reference timestamp: " + referenceTimestamp->toXsDateTime());
        std::shared_ptr<const fhirtools::FhirStructureRepository> repoView;
        repoView = viewList.match(std::addressof(fhirInstance.backend()), key.url, *key.version);

        setProfile(value(model::profileWithVersion(ProfileType::MedicationDispenseBundle, *repoView)));
    }
}

std::optional<model::Timestamp> MedicationDispenseBundle::getValidationReferenceTimestamp() const
{
    const auto medications = getResourcesByType<model::MedicationDispense>();
    ErpExpect(! medications.empty(), HttpStatus::BadRequest,
              "Expected at least one MedicationDispense in MedicationDispenseBundle");
    return medications[0].whenHandedOver();
}

}// namespace model
