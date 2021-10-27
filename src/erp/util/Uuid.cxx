/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Gsl.hxx"
#include "erp/util/OpenSsl.hxx"
#include "erp/util/String.hxx"
#include "erp/util/Uuid.hxx"

#include <cstdio>
#include <regex>


Uuid::Uuid ()
{
    union
    {
        std::uint8_t bytes[16];
        struct
        {
            std::uint32_t timeLow;
            std::uint16_t timeMid;
            std::uint16_t timeHiAndVersion;
            std::uint8_t clockSeqHiAndReserved;
            std::uint8_t clockSeqLow;
            std::uint8_t node[6];
        } parts;
    } uuid = {{0}};

    static_assert(sizeof uuid.bytes == sizeof uuid.parts);

    // Spec: https://tools.ietf.org/html/rfc4122#section-4.4
    // o  Set all the other bits to randomly (or pseudo-randomly) chosen
    //    values.
    if (!RAND_bytes(uuid.bytes, sizeof uuid))
    {
        throw std::runtime_error("Could not generate random bytes");
    }

    // o  Set the two most significant bits (bits 6 and 7) of the
    //    clock_seq_hi_and_reserved to zero and one, respectively.
    uuid.parts.clockSeqHiAndReserved &= 0x3fu;
    uuid.parts.clockSeqHiAndReserved |= 0x80u;

    // o  Set the four most significant bits (bits 12 through 15) of the
    //    time_hi_and_version field to the 4-bit version number from
    //    Section 4.1.3.
    uuid.parts.timeHiAndVersion &= 0x0fffu;
    uuid.parts.timeHiAndVersion |= 0x4000u;

    mString.assign(36, ' ');
    // Writing to the trailing null byte of std::string is valid as long as we only overwrite it
    // with char() == '\0' (which snprintf does).
    std::snprintf(mString.data(), mString.size() + 1,
                  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  uuid.parts.timeLow,
                  uuid.parts.timeMid,
                  uuid.parts.timeHiAndVersion,
                  uuid.parts.clockSeqHiAndReserved,
                  uuid.parts.clockSeqLow,
                  uuid.parts.node[0],
                  uuid.parts.node[1],
                  uuid.parts.node[2],
                  uuid.parts.node[3],
                  uuid.parts.node[4],
                  uuid.parts.node[5]);
}


Uuid::Uuid (std::string_view stringRepresentation)
{
    if (stringRepresentation.find("urn:uuid:")==0)
        mString = stringRepresentation.substr(9);
    else
        mString = stringRepresentation;
}


bool Uuid::isValidIheUuid () const
{
    static const std::regex re("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return std::regex_match(mString, re);
}


const std::string& Uuid::toString () const
{
    return mString;
}
Uuid::operator const std::string&() const
{
    return mString;
}


std::string Uuid::toUrn () const
{
    if (isValidIheUuid())
        return "urn:uuid:" + mString;
    else
        return mString;
}


std::string Uuid::toOid() const
{
    Expects(isValidIheUuid());

    // remove '-'s and concatenate
    std::string hex = String::concatenateStrings(String::split(mString, '-'), "");
    const char* p_str = hex.c_str();
    BIGNUM* bigNumber = nullptr;
    BN_hex2bn(&bigNumber, p_str);
    std::shared_ptr<BIGNUM> bigNumberPtr(bigNumber, BN_free);
    auto dec = std::shared_ptr<char>(
        BN_bn2dec(bigNumberPtr.get()),
        [](char*p){OPENSSL_free(p);});
    std::string decString = std::string(dec.get());
    return "2.25." + decString;
}


bool operator== (const Uuid& lhs, const Uuid& rhs)
{
    return lhs.toString() == rhs.toString();
}

bool operator!=(const Uuid& lhs, const Uuid& rhs)
{
    return lhs.toString() != rhs.toString();
}


bool operator< (const Uuid& lhs, const Uuid& rhs)
{
    return lhs.toString() < rhs.toString();
}
