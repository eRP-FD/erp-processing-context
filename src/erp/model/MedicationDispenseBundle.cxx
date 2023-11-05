/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispense.hxx"

namespace model
{

std::optional<model::Timestamp> MedicationDispenseBundle::getValidationReferenceTimestamp() const
{
    const auto medications = getResourcesByType<model::MedicationDispense>();
    ErpExpect(! medications.empty(), HttpStatus::BadRequest,
              "Expected at least one MedicationDispense in MedicationDispenseBundle");
    return medications[0].whenHandedOver();
}

}// namespace model
