/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "EpaOpCancelPrescriptionErpInputParameters.hxx"
#include "shared/model/PrescriptionId.hxx"

namespace model
{

EPAOpCancelPrescriptionERPInputParameters::EPAOpCancelPrescriptionERPInputParameters(
    const model::PrescriptionId& prescriptionId, const model::Timestamp& authoredOn)
    : Parameters<EPAOpCancelPrescriptionERPInputParameters>(profileType)
{
    setResourceType("Parameters");
    auto& rxPrescriptionParameter = findOrAddParameter(resource::parameter::rxPrescription);
    auto& prescriptionIdPart = findOrAddPart(rxPrescriptionParameter, resource::parameter::part::prescriptionId);
    setValueIdentifier(prescriptionIdPart, resource::naming_system::prescriptionID, prescriptionId.toString());
    auto& authoredOnPart = findOrAddPart(rxPrescriptionParameter, resource::parameter::part::authoredOn);
    setValueDate(authoredOnPart, authoredOn.toGermanDate());
}

std::optional<Timestamp> EPAOpCancelPrescriptionERPInputParameters::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

template class Parameters<EPAOpCancelPrescriptionERPInputParameters>;
}
