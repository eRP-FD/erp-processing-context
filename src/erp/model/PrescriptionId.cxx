/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "PrescriptionId.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <iomanip>

namespace model
{

PrescriptionId PrescriptionId::fromDatabaseId (const PrescriptionType prescriptionType, const int64_t databaseId)
{
    return PrescriptionId(prescriptionType, databaseId, calculateChecksum(prescriptionType, databaseId));
}

PrescriptionId PrescriptionId::fromString (const std::string_view prescriptionId)
{
    const auto parts = String::split(prescriptionId, '.');

    ModelExpect(parts.size() == 6u, "Wrong format of Prescription ID: " + std::string(prescriptionId));
    for (size_t i = 0; i < 5u; ++i)
    {
        ModelExpect(parts[i].size() == 3, "Wrong format of Prescription ID: " + std::string(prescriptionId));
    }
    ModelExpect(parts[5].size() == 2, "Wrong format of Prescription ID: " + std::string(prescriptionId));

    uint8_t part0 = static_cast<uint8_t>(std::stoi(parts[0]));
    const auto type = magic_enum::enum_cast<PrescriptionType>(part0);
    ModelExpect(type.has_value(), "Unsupported prescription type " + parts[0]);


    int64_t id = std::stoll(parts[1]) * 1'000'000'000
               + std::stoll(parts[2]) * 1'000'000
               + std::stoll(parts[3]) * 1'000
               + std::stoll(parts[4]);

    const uint8_t checksum = static_cast<uint8_t>(std::stoi(parts[5]));

    A_19218.start("Validate the incoming checksum");
    validateChecksum(*type, id, checksum);
    A_19218.finish();

    return PrescriptionId(*type, id, checksum);
}

PrescriptionId::PrescriptionId (const PrescriptionType prescriptionType, const int64_t id, const uint8_t checksum)
        : mPrescriptionType(prescriptionType), mId(id), mChecksum(checksum)
{
}

int64_t PrescriptionId::toDatabaseId () const
{
    return mId;
}

std::string PrescriptionId::toString () const
{
    // build string representation of format: aaa.bbb.bbb.bbb.bbb.cc
    // where:
    // aaa = type
    // bbb.bbb.bbb.bbb = ID
    // cc = checksum

    A_19217.start("Prescription type: aaa");
    std::ostringstream oss;
    oss << std::setfill('0');
    oss << std::setw(3) << static_cast<uint16_t>(mPrescriptionType);
    A_19217.finish();

    oss << '.';

    A_19217.start("Ongoing prescription ID: bbb.bbb.bbb.bbb");
    class PrescriptionIdNumpunct : public std::numpunct<char>
    {
    protected:
        char do_thousands_sep () const override { return '.'; }
        std::string do_grouping () const override { return "\03"; }
        char do_decimal_point () const override { return ','; }
    };
    std::ostringstream ossId;
    ossId.imbue(std::locale(std::locale::classic(), new PrescriptionIdNumpunct));
    ossId << mId;

    const std::string tail = ossId.str();
    std::string strId = "000.000.000.000";

    ModelExpect(tail.size() <= strId.size(),
                "Error converting database ID " + std::to_string(mId) +
                    " to Prescription ID representation. Database ID is to long");

    strId.replace(strId.size() - tail.size(), tail.size(), tail);

    oss << strId;
    A_19217.finish();

    oss << '.';

    A_19217.start("Checksum");
    oss << std::setw(2) << static_cast<uint16_t>(mChecksum);
    A_19217.finish();

    return oss.str();
}

std::string PrescriptionId::deriveUuid(const uint8_t& feature) const
{
    boost::uuids::uuid uuid{};
    static_assert(sizeof uuid.data == 16, "did the boost implementation change?");
    static_assert(sizeof(mPrescriptionType) + sizeof(mId) + sizeof(feature) <= sizeof(uuid.data),
                  "not enough bytes in uuid");

    std::memcpy(uuid.data, &mPrescriptionType, sizeof(mPrescriptionType));
    std::memcpy(uuid.data + sizeof(mPrescriptionType), &mId, sizeof(mId));
    std::memcpy(uuid.data + sizeof(mPrescriptionType) + sizeof(mId), &feature, sizeof(feature));

    return boost::uuids::to_string(uuid);
}

PrescriptionType PrescriptionId::type() const
{
    return mPrescriptionType;
}

void PrescriptionId::validateChecksum (const PrescriptionType prescriptionType, const int64_t id, const uint8_t checksum)
{
    A_19218.start("Validate the incoming checksum");
    int64_t toBeChecked = static_cast<int64_t>(prescriptionType) * 100'000'000'000'000
            + id * 100
            + checksum;

    if (toBeChecked % 97 != 1)
    {
        const auto calculatedChecksum = calculateChecksum(prescriptionType, id);
        ModelExpect(toBeChecked % 97 == 1,
                "Checksum " + std::to_string(checksum) + " for prescription ID " + std::to_string(id)
                        + " is wrong. Should be: " + std::to_string(calculatedChecksum));
    }
}

uint8_t PrescriptionId::calculateChecksum (const PrescriptionType prescriptionType, const int64_t id)
{
    A_19217.start("Checksum according to [ISO 7064]");
    const auto type = static_cast<int64_t>(prescriptionType) * 1'000'000'000'000;
    return 98 - (((type + id) * 100) % 97);
}

bool PrescriptionId::operator==(const PrescriptionId& rhs) const
{
    return mPrescriptionType == rhs.mPrescriptionType && mId == rhs.mId && mChecksum == rhs.mChecksum;
}

bool PrescriptionId::operator!=(const PrescriptionId& rhs) const
{
    return ! (rhs == *this);
}

}

