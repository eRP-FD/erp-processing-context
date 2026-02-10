// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/model/trezept/ErpTPrescriptionCarbonCopy.hxx"
#include "shared/model/PrescriptionId.hxx"

namespace model
{

ErpTPrescriptionCarbonCopy::ErpTPrescriptionCarbonCopy()
    : Parameters(profileType)
{
    setResourceType("Parameters");
}
void ErpTPrescriptionCarbonCopy::setPrescriptionSignatureDate(const Timestamp& prescriptionSignatureDate)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::prescriptionSignatureDate);
    setValueInstant(part, prescriptionSignatureDate.toXsDateTime());
}

void ErpTPrescriptionCarbonCopy::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::prescriptionId);
    setValueIdentifier(part, resource::naming_system::prescriptionID, prescriptionId.toString());
}

void ErpTPrescriptionCarbonCopy::setMedicationRequest(const rapidjson::Value& medicationRequest)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::medicationRequest);
    setResource(part, medicationRequest);
}

void ErpTPrescriptionCarbonCopy::setMedication(const rapidjson::Value& medication)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& part = findOrAddPart(parameter, resource::parameter::part::medication);
    setResource(part, medication);
}

void ErpTPrescriptionCarbonCopy::addDispenseInformation(const rapidjson::Value& medicationDispense,
                                                        const rapidjson::Value& medication)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& dispenseInformation = addPart(parameter, resource::parameter::part::dispenseInformation);
    auto& medicationDispensePart = addPart(dispenseInformation, resource::parameter::part::medicationDispense);
    setResource(medicationDispensePart, medicationDispense);
    auto& medicationPart = addPart(dispenseInformation, resource::parameter::part::medication);
    setResource(medicationPart, medication);
}

void ErpTPrescriptionCarbonCopy::setOrganization(const rapidjson::Value& organization)
{
    auto& parameter = findOrAddParameter(resource::parameter::rxDispensation);
    auto& part = findOrAddPart(parameter, resource::parameter::part::dispenseOrganization);
    setResource(part, organization);
}

const rapidjson::Value& ErpTPrescriptionCarbonCopy::rxPrescriptionMedication() const
{
    const auto* parameter = findParameter(resource::parameter::rxPrescription);
    ModelExpect(parameter, "could not find rxPrescription parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::medication);
    ModelExpect(part, "could not find part " + std::string{resource::parameter::part::medication});
    const auto* medication = getResource(*part);
    ModelExpect(medication, "Medication not present in medication part");
    return *medication;
}

std::tuple<gsl::not_null<const rapidjson::Value*>, gsl::not_null<const rapidjson::Value*>>
ErpTPrescriptionCarbonCopy::rxDispensationInformation(size_t idx) const
{
    const auto* parameter = findParameter(resource::parameter::rxDispensation);
    ModelExpect(parameter, "could not find rxDispensation parameter");
    const auto dispenseInformation = findParts(*parameter, resource::parameter::part::dispenseInformation);
    ModelExpect(! dispenseInformation.empty(), "could not find dispenseInformation part");
    ModelExpect(idx < dispenseInformation.size(), "dispenseInformation index out of range");
    const auto* medicationDispensePart =
        findPart(*dispenseInformation[idx], resource::parameter::part::medicationDispense);
    ModelExpect(medicationDispensePart, "could not find medicationDispense part");
    const auto* medicationPart = findPart(*dispenseInformation[idx], resource::parameter::part::medication);
    ModelExpect(medicationPart, "could not find medication part");
    const auto* medicationDispense = getResource(*medicationDispensePart);
    ModelExpect(medicationDispense, "MedicationDispense not present in medicationDispense part");
    const auto* medication = getResource(*medicationPart);
    ModelExpect(medication, "Medication not present in medication part");
    return std::make_tuple(medicationDispense, medication);
}

const rapidjson::Value& ErpTPrescriptionCarbonCopy::organization() const
{
    const auto* parameter = findParameter(resource::parameter::rxDispensation);
    ModelExpect(parameter, "could not find rxDispensation parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::dispenseOrganization);
    ModelExpect(part, "could not find part " + std::string{resource::parameter::part::dispenseOrganization});
    const auto* organization = getResource(*part);
    ModelExpect(organization, "Organization not present in dispenseOrganization part");
    return *organization;
}

Timestamp ErpTPrescriptionCarbonCopy::prescriptionSignatureDate() const
{
    const auto* parameter = findParameter(resource::parameter::rxPrescription);
    ModelExpect(parameter, "could not find rxPrescription parameter");
    const auto* part = findPart(*parameter, resource::parameter::part::prescriptionSignatureDate);
    ModelExpect(part, "could not find part " + std::string{resource::parameter::part::prescriptionSignatureDate});
    return getValueInstant(*part);
}

template class Parameters<ErpTPrescriptionCarbonCopy>;

}
