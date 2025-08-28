/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/EuMedicationDispenseInfos.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"

namespace model
{

EuMedicationDispenseInfos::EuMedicationDispenseInfos(GemErpEuPrPractitioner&& practitioner,
                                                     GemErpEuPrPractitionerRole&& practitionerRole,
                                                     GemErpEuPrOrganization&& organization)
    : mPractitioner(std::move(practitioner))
    , mPractitionerRole(std::move(practitionerRole))
    , mOrganization(std::move(organization))
{
}

const GemErpEuPrPractitioner& EuMedicationDispenseInfos::practitioner() const
{
    return mPractitioner;
}

const GemErpEuPrPractitionerRole& EuMedicationDispenseInfos::practitionerRole() const
{
    return mPractitionerRole;
}

const GemErpEuPrOrganization& EuMedicationDispenseInfos::organization() const
{
    return mOrganization;
}

std::optional<EuMedicationDispenseInfos> EuMedicationDispenseInfos::create(const MedicationDispenseBundle& bundle)
{
    using namespace model::resource;
    static const rapidjson::Pointer resourcePointer{ElementName::path(elements::resource)};
    static const rapidjson::Pointer resourceTypePointer{ElementName::path(elements::resourceType)};
    GemErpEuPrPractitioner practitioner;
    GemErpEuPrPractitionerRole practitionerRole;
    GemErpEuPrOrganization organization;
    for (const auto& entry : bundle.getEntries())
    {
        const auto* resource = resourcePointer.Get(entry);
        ModelExpect(resource != nullptr, "missing resource in Bundle.entry");
        const auto* resourceTypeValue = resourceTypePointer.Get(*resource);
        ModelExpect(resourceTypeValue != nullptr, "missing resourceType in Bundle.entry.resource");
        const auto& resourceType = NumberAsStringParserDocument::getStringValueFromValue(resourceTypeValue);
        if (resourceType == MedicationDispense::resourceTypeName ||
            resourceType == GemErpPrMedication::resourceTypeName)
        {
            // Handled in MedicationsAndDispenses
        }
        else if (resourceType == GemErpEuPrPractitioner::resourceTypeName)
        {
            practitioner = GemErpEuPrPractitioner::fromJson(*resource);
        }
        else if (resourceType == GemErpEuPrPractitionerRole::resourceTypeName)
        {
            practitionerRole = GemErpEuPrPractitionerRole::fromJson(*resource);
        }
        else if (resourceType == GemErpEuPrOrganization::resourceTypeName)
        {
            organization = GemErpEuPrOrganization::fromJson(*resource);
        }
        else
        {
            ModelFail("Unexpected resource type in MedicationDispenseBundle: " + std::string(resourceType));
        }
    }
    if (practitioner.getId().has_value())
    {
        return EuMedicationDispenseInfos(std::move(practitioner), std::move(practitionerRole), std::move(organization));
    }
    return std::nullopt;
}

}
