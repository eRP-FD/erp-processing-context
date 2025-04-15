/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/Uuid.hxx"
#include "library/json/JsonReader.hxx"
#include "library/json/JsonWriter.hxx"
#include "library/util/Assert.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <boost/regex.hpp>
#include <boost/uuid/name_generator_md5.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <ostream>

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/String.hxx"

namespace epa
{

namespace
{
    std::string randomUuid()
    {
        boost::uuids::uuid uuid{};

        // Spec: https://tools.ietf.org/html/rfc4122#section-4.4
        // o  Set all the other bits to randomly (or pseudo-randomly) chosen
        //    values.
        if (! RAND_bytes(uuid.data, sizeof uuid.data))
            throw std::runtime_error("Could not generate random bytes");

        // o  Set the two most significant bits (bits 6 and 7) of the
        //    clock_seq_hi_and_reserved to zero and one, respectively.
        uuid.data[8] &= 0x3fu;
        uuid.data[8] |= 0x80u;
        Assert(uuid.variant() == boost::uuids::uuid::variant_rfc_4122);

        // o  Set the four most significant bits (bits 12 through 15) of the
        //    time_hi_and_version field to the 4-bit version number from
        //    Section 4.1.3.
        uuid.data[6] &= 0x0fu;
        uuid.data[6] |= 0x40u;
        Assert(uuid.version() == boost::uuids::uuid::version_random_number_based);

        return to_string(uuid);
    }

    std::string removePrefix(const std::string& stringRepresentation)
    {
        if (stringRepresentation.starts_with("urn:uuid:"))
            return stringRepresentation.substr(9);
        else
            return stringRepresentation;
    }
}


Uuid::Uuid()
  : mUuid{randomUuid()}
{
}


Uuid::Uuid(const std::string& stringRepresentation)
  : mUuid(removePrefix(stringRepresentation))
{
}


Uuid::Uuid(const Uuid&) = default;


Uuid::Uuid(Uuid&&) noexcept = default;


Uuid::~Uuid() = default;


Uuid& Uuid::operator=(const Uuid&) = default;


Uuid& Uuid::operator=(Uuid&&) noexcept = default;


Uuid Uuid::nameUuidV3(const Uuid& namespacePrefix, std::string_view name)
{
    const boost::uuids::string_generator stringGenerator;
    const auto namespaceUuid = stringGenerator(namespacePrefix.toString());
    const boost::uuids::name_generator_md5 nameGenerator{namespaceUuid};
    return Uuid{to_string(nameGenerator(name.data(), name.size()))};
}


// GEMREQ-start A_14760-20#isValidIheUuid
bool Uuid::isValidIheUuid() const
{
    static const boost::regex re("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return boost::regex_match(mUuid, re);
}
// GEMREQ-end A_14760-20#isValidIheUuid


const std::string& Uuid::toString() const
{
    return mUuid;
}


std::string Uuid::toUrn() const
{
    if (isValidIheUuid())
        return "urn:uuid:" + mUuid;
    else
        return mUuid;
}


std::string Uuid::toOid() const
{
    Assert(isValidIheUuid()) << "Only valid UUIDs can be converted to an OID";

    // Remove '-' characters:
    const std::string hex = String::replaceAll(mUuid, "-", "");
    // Convert UUID as long hexadecimal number to OpenSSL BIGNUM:
    BIGNUM* bigNumber = nullptr;
    BN_hex2bn(&bigNumber, hex.c_str());
    const auto freeBigNumber = gsl::finally([bigNumber] { BN_free(bigNumber); });
    // Convert back to decimal string:
    char* dec = BN_bn2dec(bigNumber);
    const auto freeDec = gsl::finally([dec] { OPENSSL_free(dec); });
    return std::string{"2.25."} + dec;
}


std::array<std::uint8_t, 16> Uuid::toBinary() const
{
    const boost::uuids::uuid uuid = boost::uuids::string_generator()(mUuid);
    std::array<std::uint8_t, 16> result{};
    std::copy(uuid.begin(), uuid.begin() + 16, result.begin());
    return result;
}


Uuid Uuid::fromJson(const JsonReader& reader)
{
    return Uuid{reader.stringValue()};
}


void Uuid::writeJson(JsonWriter& writer) const
{
    writer.makeStringValue(toString());
}


size_t Uuid::getHashValue() const noexcept
{
    static constexpr std::hash<std::string> hash{};
    return hash(mUuid);
}

std::ostream& operator<<(std::ostream& stream, const Uuid& uuid)
{
    return stream << uuid.toString();
}

} // namespace epa
