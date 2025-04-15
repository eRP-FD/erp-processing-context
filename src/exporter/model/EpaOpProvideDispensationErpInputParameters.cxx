/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/EpaOpProvideDispensationErpInputParameters.hxx"
#include "shared/model/PrescriptionId.hxx"

namespace model
{

EPAOpProvideDispensationERPInputParameters::EPAOpProvideDispensationERPInputParameters()
    : Parameters<EPAOpProvideDispensationERPInputParameters>(profileType)
{
    setResourceType("Parameters");
}

void EPAOpProvideDispensationERPInputParameters::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& part = findOrAddPart(parameter, resource::parameter::part::prescriptionId);
    setValueIdentifier(part, resource::naming_system::prescriptionID, prescriptionId.toString());
}

void EPAOpProvideDispensationERPInputParameters::setAuthoredOn(const Timestamp& authoredOn)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& part = findOrAddPart(parameter, resource::parameter::part::authoredOn);
    setValueDate(part, authoredOn.toGermanDate());
}

void EPAOpProvideDispensationERPInputParameters::addMedicationAndDispense(const rapidjson::Value& medicationDispense,
                                                                          const rapidjson::Value& medication)
{
    addMedicationDispense(medicationDispense);
    addMedication(medication);
}

void EPAOpProvideDispensationERPInputParameters::addMedicationDispense(const rapidjson::Value& medicationDispense)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& medicationDispensePart = addPart(parameter, resource::parameter::part::medicationDispense);
    setResource(medicationDispensePart, medicationDispense);
}

void EPAOpProvideDispensationERPInputParameters::addMedication(const rapidjson::Value& medication)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& medicationPart = addPart(parameter, resource::parameter::part::medication);
    setResource(medicationPart, medication);
}

void EPAOpProvideDispensationERPInputParameters::setOrganization(const rapidjson::Value& organization)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& part = findOrAddPart(parameter, resource::parameter::part::organization);
    setResource(part, organization);
}

std::optional<Timestamp> EPAOpProvideDispensationERPInputParameters::getValidationReferenceTimestamp() const
{
    return FhirResourceBase::getValidationReferenceTimestamp();
}

std::vector<const rapidjson::Value*> EPAOpProvideDispensationERPInputParameters::getMedicationDispenses() const
{
    const auto* parameter = findParameter(resource::parameter::rxDispensation);
    ModelExpect(parameter, "could not find rxDispensation parameter");
    const auto parts = findParts(*parameter, resource::parameter::part::medicationDispense);
    std::vector<const rapidjson::Value*> medicationDispenses;
    medicationDispenses.reserve(parts.size());
    for (const auto& part : parts)
    {
        medicationDispenses.emplace_back(getResource(*part));
    }
    return medicationDispenses;
}

std::vector<const rapidjson::Value*> EPAOpProvideDispensationERPInputParameters::getMedications() const
{
    const auto* parameter = findParameter(resource::parameter::rxDispensation);
    ModelExpect(parameter, "could not find rxDispensation parameter");
    const auto parts = findParts(*parameter, resource::parameter::part::medication);
    std::vector<const rapidjson::Value*> medications;
    medications.reserve(parts.size());
    for (const auto& part : parts)
    {
        medications.emplace_back(getResource(*part));
    }
    return medications;
}

const rapidjson::Value* EPAOpProvideDispensationERPInputParameters::getOrganization() const
{
    const auto* parameter = findParameter(resource::parameter::rxDispensation);
    ModelExpect(parameter, "could not find rxDispensation parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::organization);
    ModelExpect(part, "could not find part " + std::string{resource::parameter::part::organization});
    return Parameters::getResource(*part);
}

template class Parameters<EPAOpProvideDispensationERPInputParameters>;
}
