/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee/KeyId.hxx"
#include "library/crypto/MemoryUtility.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"

#include <algorithm>
#include <stdexcept>

namespace epa
{

KeyId KeyId::fromBinaryString(std::string_view binaryString)
{
    KeyId result{};
    Assert(binaryString.length() == result.bytes.size())
        << "KeyId constructed from wrong size: " << binaryString.length();
    // char is potentially signed, but converting signed to unsigned is well-defined.
    std::copy(binaryString.begin(), binaryString.end(), result.bytes.begin());
    return result;
}


KeyId KeyId::fromBinaryView(BinaryView binaryView)
{
    KeyId result{};
    Assert(binaryView.size_bytes() == result.bytes.size())
        << "KeyId constructed from wrong size: " << binaryView.size_bytes();
    // char is potentially signed, but converting signed to unsigned is well-defined.
    std::copy(binaryView.begin(), binaryView.end(), result.bytes.begin());
    return result;
}


KeyId KeyId::fromHexString(const std::string_view value)
{
    return fromBinaryString(ByteHelper::fromHex(value));
}


KeyId::~KeyId()
{
    MemoryUtility::cleanse(bytes);
}


std::string KeyId::toBinaryString() const
{
    const char* chars = reinterpret_cast<const char*>(bytes.data());
    return std::string(chars, chars + bytes.size());
}


std::string KeyId::toHexString() const
{
    return ByteHelper::toHex(BinaryView{bytes.data(), bytes.size()});
}


std::string KeyId::toHexString(const std::optional<KeyId>& keyId)
{
    return keyId ? keyId->toHexString() : "(none)";
}


bool operator==(const KeyId& lhs, const KeyId& rhs)
{
    return lhs.bytes == rhs.bytes;
}


bool operator!=(const KeyId& lhs, const KeyId& rhs)
{
    return ! (lhs == rhs);
}


std::ostream& operator<<(std::ostream& os, const KeyId& rhs)
{
    return os << rhs.toHexString();
}

} // namespace epa

std::size_t std::hash<epa::KeyId>::operator()(const epa::KeyId& keyId) const
{
    const char* chars = reinterpret_cast<const char*>(keyId.bytes.data());
    std::string_view binaryStringView(chars, keyId.bytes.size());
    return std::hash<std::string_view>{}(binaryStringView);
}
