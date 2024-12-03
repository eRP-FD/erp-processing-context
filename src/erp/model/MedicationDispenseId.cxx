/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/MedicationDispenseId.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"


namespace model
{

static constexpr auto medication_dispense_separator = '-';

MedicationDispenseId MedicationDispenseId::fromString(const std::string_view& str)
{
    const auto parts = String::split(str, medication_dispense_separator);
    const auto numParts = parts.size();
    ModelExpect(numParts == 1 || numParts == 2, "invalid medication dispense ID: " + std::string(str));
    size_t ret = 0;
    if (numParts == 2)
    {
        ModelExpect(! parts[1].empty(), "invalid medication dispense ID: " + std::string(str));
        try
        {
            size_t idx = 0;
            ret = std::stoull(parts[1], &idx);
            ModelExpect(idx == parts[1].size(), "invalid medication dispense ID: " + std::string(str));
            ModelExpect(ret > 0, "invalid medication dispense ID: " + std::string(str));
        }
        catch (const std::exception& ex)
        {
            ModelFail("invalid medication dispense ID: " + std::string(str) + " - " + ex.what());
        }
    }
    return MedicationDispenseId(PrescriptionId::fromString(parts[0]), ret);
}

MedicationDispenseId::MedicationDispenseId(const PrescriptionId& prescriptionId, size_t index)
    : mPrescriptionId(prescriptionId)
    , mIndex(index)
{
}

std::string MedicationDispenseId::toString() const
{
    if (mIndex > 0)
    {
        return mPrescriptionId.toString() + medication_dispense_separator + std::to_string(mIndex);
    }
    return mPrescriptionId.toString();
}

const PrescriptionId& MedicationDispenseId::getPrescriptionId() const
{
    return mPrescriptionId;
}

size_t MedicationDispenseId::getIndex() const
{
    return mIndex;
}

bool MedicationDispenseId::operator==(const MedicationDispenseId& rhs) const
{
    return std::tie(mPrescriptionId, mIndex) == std::tie(rhs.mPrescriptionId, rhs.mIndex);
}

bool MedicationDispenseId::operator!=(const MedicationDispenseId& rhs) const
{
    return ! (rhs == *this);
}

}
