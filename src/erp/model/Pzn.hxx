/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_PZN_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_PZN_HXX

#include <string>
#include <string_view>

namespace model
{

class Pzn
{
public:
    explicit Pzn(std::string pzn);
    explicit Pzn(const char* pzn);
    Pzn(std::string_view pzn);

    /** return the string representation of the pzn
     */
    const std::string& id() const&;

    /** Moves stored value */
    std::string id() &&;

    void setId(std::string_view id);

    static bool isPzn(const std::string& value);

    /**
     * Returns true if the stored pzn is valid in the sense that the format
     * is correct, the checksum is not tested.
     */
    bool validFormat() const;

    bool validChecksum() const;

    friend bool operator==(const Pzn& lhs, const Pzn& rhs) = default;

private:
    std::string mValue;
};

bool operator==(const Pzn& lhs, std::string_view rhs);
std::ostream& operator<<(std::ostream& os, const Pzn& pzn);

}// namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_LANR_HXX