/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_UUID_HXX
#define EPA_LIBRARY_UTIL_UUID_HXX

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace epa
{

class Uuid
{
public:
    /**
     * Uses the OpenSSL secure random library to create a new V4 UUID.
     * The UUID will conform to IHE ITI TF3, 4.2.3.1.5.
     *
     * @throw std::runtime_error when no random data can be generated
     */
    Uuid();

    /**
     * Parses a UUID from a string representation. A leading urn:uuid: prefix will be automatically
     * removed.
     */
    explicit Uuid(const std::string& stringRepresentation);

    Uuid(const Uuid&);
    Uuid(Uuid&&) noexcept;
    ~Uuid();
    Uuid& operator=(const Uuid&);
    Uuid& operator=(Uuid&&) noexcept;

    /**
     * Creates a type-3 UUID (MD5 based) from the given string.
     *
     * See https://www.rfc-editor.org/rfc/rfc4122#section-4.3 for the definition of a name UUID,
     * and the meaning of the namespacePrefix parameter.
     */
    static Uuid nameUuidV3(const Uuid& namespacePrefix, std::string_view name);

    /**
     * Validates the format of a UUID according to IHE ITI TF3, 4.2.3.1.5 (must be all-lowercase).
     */
    bool isValidIheUuid() const;

    /**
     * @return the string representation of this UUID.
     */
    const std::string& toString() const;

    /**
     * Encodes the UUID in a form that is useful inside IHE-specific XML structures.
     *
     * @return "urn:uuid:" + toString()
     */
    std::string toUrn() const;

    /**
     * Converts the hex representation of the UUID to dec
     * as documented in IHE_ITI_TF_Vol2x Appendix B.6
     *
     * @return "2.25." + UUID as decimal uint
     */
    std::string toOid() const;

    std::array<std::uint8_t, 16> toBinary() const;

    static Uuid fromJson(const class JsonReader& reader);

    /** Writes the UUID, as a string (no URN prefix). */
    void writeJson(class JsonWriter& writer) const;

    bool operator==(const Uuid& other) const = default;
    auto operator<=>(const Uuid& other) const = default;

    size_t getHashValue() const noexcept;

private:
    std::string mUuid;
};


std::ostream& operator<<(std::ostream& stream, const Uuid& uuid);

} // namespace epa

namespace std
{
template<>
struct hash<epa::Uuid>
{
    std::size_t operator()(const epa::Uuid& uuid) const noexcept
    {
        return uuid.getHashValue();
    }
};
}


#endif
