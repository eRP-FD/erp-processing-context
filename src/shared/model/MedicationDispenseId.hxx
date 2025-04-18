/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEID_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEID_HXX

#include "shared/model/PrescriptionId.hxx"

namespace model
{

class MedicationDispenseId
{
public:
    // initialize from medication dispense id, e.g. from aaa.bbb.bbb.bbb.bbb.cc or aaa.bbb.bbb.bbb.bbb.cc-d
    [[nodiscard]] static MedicationDispenseId fromString(const std::string_view& str);
    // MedicationDispense IDs are appended with index, because more than one may exist per task.
    MedicationDispenseId(const PrescriptionId& prescriptionId, size_t index);
    MedicationDispenseId(const MedicationDispenseId&) = default;
    [[nodiscard]] std::string toString() const;

    [[nodiscard]] const PrescriptionId& getPrescriptionId() const;
    [[nodiscard]] size_t getIndex() const;

    [[nodiscard]] bool operator==(const MedicationDispenseId& rhs) const;
    [[nodiscard]] bool operator!=(const MedicationDispenseId& rhs) const;

private:
    const PrescriptionId mPrescriptionId;
    const size_t mIndex;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEID_HXX
