/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_LANR_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_LANR_HXX

#include <string>
#include <string_view>

namespace model
{

/**
 * Represents the "lebenslange Arztnummer" (LANR)
 */
class Lanr
{
public:
    enum class Type
    {
        unspecified,
        lanr,
        zanr,
    };
    explicit Lanr(std::string lanr, Type type = Type::unspecified);
    explicit Lanr(const char* lanr, Type type = Type::unspecified);
    Lanr(std::string_view lanr, Type type = Type::unspecified);

    /** return the string representation of the lanr
     */
    const std::string& id() const&;
    Type getType() const;

    /** Moves stored value */
    std::string id() &&;
    std::string_view namingSystem(bool deprecated) const;

    void setId(std::string_view id);

    static bool isLanr(const std::string& value);

    /**
     * Returns true if the stored lanr is valid in the sense that the format
     * is correct, the checksum is not tested.
     */
    bool validFormat() const;

    bool validChecksum() const;

    bool isPseudoLanr() const;

    friend bool operator==(const Lanr& lhs, const Lanr& rhs) = default;

private:
    std::string mValue;
    Type mType;
};

bool operator==(const Lanr& lhs, std::string_view rhs);
std::ostream& operator<<(std::ostream& os, const Lanr& lanr);

}// namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_LANR_HXX