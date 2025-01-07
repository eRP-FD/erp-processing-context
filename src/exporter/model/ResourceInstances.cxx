/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#include "exporter/model/DataAbsentReason.hxx"
#include "exporter/model/EpaMedicationPznIngredient.hxx"
#include "exporter/model/EpaMedicationTypeExtension.hxx"
#include "exporter/model/EpaOpCancelPrescriptionErpInputParameters.hxx"
#include "exporter/model/EpaOpProvideDispensationErpInputParameters.hxx"
#include "exporter/model/EpaOpProvidePrescriptionErpInputParameters.hxx"
#include "exporter/model/EpaOpRxDispensationErpOutputParameters.hxx"
#include "exporter/model/EpaOpRxPrescriptionErpOutputParameters.hxx"
#include "exporter/model/OrganizationDirectory.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Resource.txx"


namespace model
{
template class Resource<DataAbsentReason>;
template class Resource<EPAOpRxDispensationERPOutputParameters>;
template class Resource<EPAOpRxPrescriptionERPOutputParameters>;
template class Resource<EPAOpCancelPrescriptionERPInputParameters>;
template class Resource<EPAOpProvideDispensationERPInputParameters>;
template class Resource<EPAOpProvidePrescriptionERPInputParameters>;
template class Resource<OrganizationDirectory>;
template class Resource<EPAMedicationPZNIngredient>;
template class Resource<EPAMedicationTypeExtension>;
}
