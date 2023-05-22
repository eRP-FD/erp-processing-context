/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KVNR_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KVNR_HXX

#include <string>
#include <string_view>

namespace model {

/**
 * Represents the "Krankenversicherungsnummer", including the information to which type
 * of patient this is referring to.
 */
class Kvnr
{
public:
    enum class Type
    {
        unspecified,
        gkv,
        pkv
    };
    explicit Kvnr(std::string kvnr, Type type = Type::unspecified);
    explicit Kvnr(std::string_view kvnr, Type type = Type::unspecified);
    explicit Kvnr(const char* kvnr, Type type = Type::unspecified);
    Kvnr(std::string_view kvnr, std::string_view namingSystem);

    /** return the string representation of the kvnr, this does not
     * include information what type of KVNR (private or public) this is.
     */
    const std::string& id() const&;

    /** Moves stored value */
    std::string id() &&;
    std::string_view namingSystem(bool deprecated) const;

    /** get the type of Kvnr */
    Type getType() const;

    void setType(Type type);

    void setId(std::string_view id);

    static bool isKvnr(const std::string& value);

    /**
     * Returns true if the stored kvnr is valid in the sense that the format
     * is correct, the checksum is not tested.
     */
    bool valid() const;

    /**
     * Returns true if the KVNR is a verification identity, cf. A_20751
     */
    bool verificationIdentity() const;

private:
    std::string mValue;
    Type mType;
};

bool operator==(const Kvnr& lhs, const Kvnr& rhs);
bool operator==(const Kvnr& lhs, std::string_view rhs);
std::ostream& operator<<(std::ostream& os, const Kvnr& kvnr);

} // namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_KVNR_HXX