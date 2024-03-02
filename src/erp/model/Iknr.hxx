/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_IKNR_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_IKNR_HXX

#include <string>
#include <string_view>

namespace model
{

/**
 * Represents the "Institutionskennzeichen" (IK)
 */
class Iknr
{
public:
    explicit Iknr(std::string iknr);
    explicit Iknr(const char* iknr);
    Iknr(std::string_view iknr);

    /** return the string representation of the iknr
     */
    const std::string& id() const&;

    /** Moves stored value */
    std::string id() &&;
    std::string_view namingSystem(bool deprecated) const;

    void setId(std::string_view id);

    static bool isIknr(const std::string& value);

    /**
     * Returns true if the stored iknr is valid in the sense that the format
     * is correct, the checksum is not tested.
     */
    bool validFormat() const;

    /**
     * Validates the checksum and format of the iknr.
     */
    bool validChecksum() const;

    friend bool operator==(const Iknr& lhs, const Iknr& rhs) = default;

private:
    std::string mValue;
};

bool operator==(const Iknr& lhs, std::string_view rhs);
std::ostream& operator<<(std::ostream& os, const Iknr& ikrnr);

}// namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_IKNR_HXX