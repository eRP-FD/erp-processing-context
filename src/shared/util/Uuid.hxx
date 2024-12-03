/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_UUID_HXX
#define E_LIBRARY_UTIL_UUID_HXX

#include <string>


class Uuid
{
public:
    /**
     * Uses the OpenSSL secure random library to create a new V4 UUID.
     * The UUID will conform to IHE ITI TF3, 4.2.3.1.5.
     *
     * @throw std::runtime_error when no random data can be generated
     */
    Uuid ();

    /**
     * Creates a UUID from a string representation. A leading urn:uuid: prefix will be automatically
     * removed.
     */
    explicit Uuid (std::string_view stringRepresentation);

    /**
     * Validates the format of a UUID according to IHE ITI TF3, 4.2.3.1.5 (must be all-lowercase).
     */
    bool isValidIheUuid () const;

    static bool isValidUrnUuid(std::string_view stringRepresentation, bool allowUpperCase = false);

    /**
     * @return the string representation of this UUID.
     */
    const std::string& toString () const;
    operator const std::string& () const; // NOLINT

    /**
     * Encodes the UUID in a form that is useful inside IHE-specific XML structures.
     *
     * @return "urn:uuid:" + toString()
     */
    std::string toUrn () const;

    /**
     * Converts the hex representation of the UUID to dec
     * as documented in IHE_ITI_TF_Vol2x Appendix B.6
     *
     * @return "2.25." + UUID as decimal uint
     */
    std::string toOid() const;

private:
    std::string mString;
};

bool operator== (const Uuid& lhs, const Uuid& rhs);
bool operator!= (const Uuid& lhs, const Uuid& rhs);
bool operator< (const Uuid& lhs, const Uuid& rhs);

#endif
