/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PRESCRIPTIONID_HXX
#define ERP_PROCESSING_CONTEXT_PRESCRIPTIONID_HXX

#include "shared/model/PrescriptionType.hxx"
#include "shared/util/Uuid.hxx"

#include <string_view>

namespace model
{

// the prescriptionID -> UUID derivation
static constexpr uint8_t uuidFeaturePrescription = 1;
static constexpr uint8_t uuidFeatureConfirmation = 2;
static constexpr uint8_t uuidFeatureReceipt = 3;
static constexpr uint8_t uuidFeatureDispenseItem = 4;


class PrescriptionId
{
public:
    // initialize from Database ID representation, i.e. from bbbbbbbbbbbb
    static PrescriptionId fromDatabaseId (PrescriptionType prescriptionType, int64_t databaseId);

    // initialize from string representation, i.e. from aaa.bbb.bbb.bbb.bbb.cc
    static PrescriptionId fromString (std::string_view prescriptionId);

    // initialize from string representation, i.e. from aaa.bbb.bbb.bbb.bbb.cc, but does not perform a validation
    static PrescriptionId fromStringNoValidation (std::string_view prescriptionId);

    // returns the Database ID representation for the Prescription ID
    // e.g. for 160.000.000.003.123.76 it returns 3123
    // i.e. for aaa.bbb.bbb.bbb.bbb.cc it returns bbbbbbbbbbbb
    [[nodiscard]] int64_t toDatabaseId () const;

    // returns the string representation, i.e. aaa.bbb.bbb.bbb.bbb.cc
    [[nodiscard]] std::string toString () const;

    // derive a UUID from the prescription ID plus a feature for distictionalibility.
    std::string deriveUuid(const uint8_t& feature) const;

    [[nodiscard]] PrescriptionType type() const;

    bool isPkv() const;

    bool operator==(const PrescriptionId& rhs) const = default;

    PrescriptionId() = delete;
    PrescriptionId(const PrescriptionId&) noexcept = default;
    // cannot assign due to const members:
    PrescriptionId& operator = (const PrescriptionId&) = delete;
    PrescriptionId& operator = (PrescriptionId&&) = delete;

private:
    PrescriptionId (PrescriptionType prescriptionType, int64_t id, uint8_t checksum);
    static void validateChecksum (PrescriptionType prescriptionType, int64_t id, uint8_t checksum);
    static uint8_t calculateChecksum (PrescriptionType prescriptionType, int64_t id);

    const PrescriptionType mPrescriptionType;
    const int64_t mId;
    const uint8_t mChecksum;
};

}

#endif //ERP_PROCESSING_CONTEXT_PRESCRIPTIONID_HXX
