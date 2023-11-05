/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "erp/model/MedicationDispense.hxx"


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
