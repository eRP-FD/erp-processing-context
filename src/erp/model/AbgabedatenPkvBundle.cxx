/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "shared/model/MedicationDispense.hxx"


namespace model
{

std::optional<model::Timestamp> AbgabedatenPkvBundle::getValidationReferenceTimestamp() const
{
    auto medications = getResourcesByType<model::MedicationDispense>();
    ErpExpect(medications.size() == 1, HttpStatus::BadRequest,
              "Unexpected number of MedicationDispenses in AbgabedatenPkvBundle");
    try
    {
        return medications[0].whenHandedOver();
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

}// namespace model
