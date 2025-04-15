/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/EpaOpProvidePrescriptionErpInputParameters.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/ResourceNames.hxx"


namespace model
{

EPAOpProvidePrescriptionERPInputParameters::EPAOpProvidePrescriptionERPInputParameters()
    : Parameters<EPAOpProvidePrescriptionERPInputParameters>(profileType)
{
    setResourceType("Parameters");
}

void EPAOpProvidePrescriptionERPInputParameters::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::prescriptionId);
    setValueIdentifier(part, resource::naming_system::prescriptionID, prescriptionId.toString());
}

void EPAOpProvidePrescriptionERPInputParameters::setAuthoredOn(const Timestamp& authoredOn)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::authoredOn);
    setValueDate(part, authoredOn.toGermanDate());
}

void EPAOpProvidePrescriptionERPInputParameters::setMedicationRequest(const rapidjson::Value& medicationRequest)
{
    setResource(resource::parameter::part::medicationRequest, medicationRequest);
}

void EPAOpProvidePrescriptionERPInputParameters::setMedication(const rapidjson::Value& medication)
{
    setResource(resource::parameter::part::medication, medication);
}

void EPAOpProvidePrescriptionERPInputParameters::setOrganization(const rapidjson::Value& organization)
{
    setResource(resource::parameter::part::organization, organization);
}

void EPAOpProvidePrescriptionERPInputParameters::setPractitioner(const rapidjson::Value& practitioner)
{
    setResource(resource::parameter::part::practitioner, practitioner);
}

std::optional<Timestamp> EPAOpProvidePrescriptionERPInputParameters::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

PrescriptionId EPAOpProvidePrescriptionERPInputParameters::getPrescriptionId() const
{
    const auto* parameter = findParameter(resource::parameter::rxPrescription);
    ModelExpect(parameter, "could not find rxPrescription parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::prescriptionId);
    ModelExpect(part, "could not find prescriptionId part");
    auto value = getValueIdentifier(*part);
    return PrescriptionId::fromString(value);
}

Timestamp EPAOpProvidePrescriptionERPInputParameters::getAuthoredOn() const
{
    const auto* parameter = findParameter(resource::parameter::rxPrescription);
    ModelExpect(parameter, "could not find rxPrescription parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::authoredOn);
    ModelExpect(part, "could not find authoredOn part");
    auto value = getValueDate(*part);
    return Timestamp::fromGermanDate(std::string{value});
}

const rapidjson::Value* EPAOpProvidePrescriptionERPInputParameters::getMedicationRequest() const
{
    return getResource(resource::parameter::part::medicationRequest);
}

const rapidjson::Value* EPAOpProvidePrescriptionERPInputParameters::getMedication() const
{
    return getResource(resource::parameter::part::medication);
}

const rapidjson::Value* EPAOpProvidePrescriptionERPInputParameters::getOrganization() const
{
    return getResource(resource::parameter::part::organization);
}

const rapidjson::Value* EPAOpProvidePrescriptionERPInputParameters::getPractitioner() const
{
    return getResource(resource::parameter::part::practitioner);
}

void EPAOpProvidePrescriptionERPInputParameters::setResource(std::string_view name, const rapidjson::Value& resource)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, name);
    Parameters::setResource(part, resource);
}

const rapidjson::Value* EPAOpProvidePrescriptionERPInputParameters::getResource(std::string_view name) const
{
    const auto* parameter = findParameter(resource::parameter::rxPrescription);
    ModelExpect(parameter, "could not find rxPrescription parameter");
    const auto* part = findPart(*parameter, name);
    ModelExpect(part, "could not find part " + std::string{name});
    return Parameters::getResource(*part);
}

template class Parameters<EPAOpProvidePrescriptionERPInputParameters>;
}
