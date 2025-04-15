/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{

model::MedicationDispenseBundle::MedicationDispenseBundle(const std::string& linkBase,
                                                          const std::vector<model::MedicationDispense>& medicationDispenses,
                                                          const std::vector<model::GemErpPrMedication>& medications)
    : MedicationDispenseBundle{model::BundleType::searchset, ::model::FhirResourceBase::NoProfile}
{
    for (const auto& medicationDispense : medicationDispenses)
    {
        const std::string urn = linkBase + "/MedicationDispense/" + medicationDispense.id().toString();
        addResource(urn, {}, model::BundleSearchMode::match, medicationDispense.jsonDocument());
    }
    for (const auto& medication: medications)
    {
        std::string urn = "urn:uuid:";
        urn += value(medication.getId());
        addResource(urn, {}, model::BundleSearchMode::include, medication.jsonDocument());
    }
}

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
        ErpExpect(repoView != nullptr, HttpStatus::BadRequest, "Invalid meta.profile: " + std::string(*profileName));

        setProfile(value(model::profileWithVersion(ProfileType::MedicationDispenseBundle, *repoView)));
    }
}
Timestamp model::MedicationDispenseBundle::whenHandedOver() const
{
    const auto medications = getResourcesByType<model::MedicationDispense>();
    ModelExpect(! medications.empty(), "Expected at least one MedicationDispense in MedicationDispenseBundle");
    return medications[0].whenHandedOver();
}

std::optional<model::Timestamp> MedicationDispenseBundle::getValidationReferenceTimestamp() const
{
    return whenHandedOver();
}

}// namespace model
