/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE_KEYID_HXX
#define EPA_LIBRARY_CRYPTO_TEE_KEYID_HXX

#include "library/crypto/tee/TeeConstants.hxx"

#include <array>
#include <functional>
#include <optional>
#include <ostream>
#include <string>

namespace epa
{

/**
 * This represents a binary 32-byte/256-bit key identifier as used in the VAU protocol/TeeProtocol.
 */
class KeyId
{
public:
    static KeyId fromBinaryString(std::string_view binaryString);
    static KeyId fromBinaryView(BinaryView binaryView);
    static KeyId fromHexString(std::string_view value);

    /**
     * This destructor safely erases the bytes using OpenSSL.
     */
    ~KeyId();

    /**
     * Returns the contents of this KeyId as a binary std::string. Because it is tempting to log
     * the binary result, please try to pass around the KeyId as an opaque, binary buffer for as
     * long as possible.
     */
    std::string toBinaryString() const;

    std::string toHexString() const;

    static std::string toHexString(const std::optional<KeyId>& keyId);

    std::array<unsigned char, vau::keyIdLength> bytes;

private:
    /**
     * The default constructor is private to avoid use of uninitialized values.
     * Please use std::optional if you don't know the KeyId from the start.
     */
    KeyId() = default;
};

bool operator==(const KeyId& lhs, const KeyId& rhs);
bool operator!=(const KeyId& lhs, const KeyId& rhs);
std::ostream& operator<<(std::ostream& os, const KeyId& rhs);

} // namespace epa

namespace std
{
template<>
class hash<epa::KeyId>
{
public:
    std::size_t operator()(const epa::KeyId& keyId) const;
};
}


#endif
