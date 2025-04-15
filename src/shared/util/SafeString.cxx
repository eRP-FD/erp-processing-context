/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/SafeString.hxx"
#include "shared/util/Expect.hxx"

#include <openssl/crypto.h>

SafeString::SafeString (void)
    : SafeString(size_t(0))
{
}

SafeString::SafeString(const SafeString::NoZeroFillTag&, size_t size)
    : mValue(std::make_unique<char[]>(size + 1))
    , mStringLength(size)
{
    Expect3(size < std::numeric_limits<size_t>::max(), "size out of range", std::out_of_range);
    mValue[mStringLength] = '\0';
}

SafeString::SafeString(const SafeString& other)
    : SafeString(SafeString::no_zero_fill, other.size())
{
    auto* otherValuePtr = other.mValue.get();
    std::copy(otherValuePtr, otherValuePtr + other.size(), mValue.get());
}

SafeString::SafeString (const size_t size)
    : SafeString(no_zero_fill, size)
{
    memset(mValue.get(), 0x00, mStringLength);
}

SafeString::SafeString(char* value, size_t size)
    : SafeString(no_zero_fill, size)
{
    std::copy(value, value + size, mValue.get());
    OPENSSL_cleanse(value, size);
}

SafeString::SafeString(std::byte* value, size_t size)
    : SafeString(reinterpret_cast<char*>(value), size)
{
}

SafeString::SafeString(unsigned char* value, size_t size)
    : SafeString(reinterpret_cast<char*>(value), size)
{}

SafeString::SafeString (const char* value)
    : SafeString(strlen(value))
{
    std::copy(value, value + strlen(value), mValue.get());
    // Trailing zero is not necessary because mValue had already been initialized with zeros. The last of which
    // remains after the copying.
}

SafeString::SafeString (SafeString&& other) noexcept
        : mValue()
        , mStringLength(other.mStringLength)
{
    mValue.swap(other.mValue);
    other.mStringLength = 0;
}

SafeString& SafeString::operator= (SafeString&& other) noexcept
{
    if (this != &other)
    {
        safeErase();
        mValue.swap(other.mValue);
        mStringLength = other.mStringLength;
        other.mStringLength = 0;
    }
    return *this;
}

void SafeString::assignAndCleanse(char* value, size_t size)
{
    safeErase();
    mValue = std::make_unique<char[]>(size + 1);
    std::copy(value, value + size, mValue.get());
    mValue[size] = '\0';
    mStringLength = size;
    OPENSSL_cleanse(value, size);
}

SafeString::~SafeString (void)
{
    safeErase();
}

SafeString::operator const char* (void) const
{
    return c_str();
}

SafeString::operator const unsigned char* (void) const
{
    checkForTerminatingZero();
    return reinterpret_cast<const unsigned char*>(mValue.get());
}

SafeString::operator std::string_view (void) const
{
    checkForTerminatingZero();
    return { mValue.get(), mStringLength };
}

SafeString::operator std::basic_string_view<std::byte>(void) const
{
    checkForTerminatingZero();
    return {reinterpret_cast<const std::byte*>(mValue.get()), mStringLength };
}

SafeString::operator gsl::span<const char> (void) const
{
    checkForTerminatingZero();
    return { mValue.get(), mStringLength };
}

SafeString::operator char* (void)
{
    return c_str();
}

SafeString::operator std::byte*(void)
{
    checkForTerminatingZero();
    return reinterpret_cast<std::byte*>(mValue.get());
}

char* SafeString::c_str()
{
    checkForTerminatingZero();
    return mValue.get();
}

const char* SafeString::c_str() const
{
    checkForTerminatingZero();
    return mValue.get();
}

size_t SafeString::size (void) const
{
    return mStringLength;
}

bool SafeString::operator==(const SafeString& other) const
{
    return mStringLength == other.mStringLength
        && std::equal(mValue.get(), mValue.get() + mStringLength, other.mValue.get());
}

bool SafeString::operator!=(const SafeString& other) const
{
    return not (*this == other);
}

bool SafeString::operator < (const SafeString& other) const
{
    return std::lexicographical_compare(
        mValue.get(),
        mValue.get() + mStringLength,
        other.mValue.get(),
        other.mValue.get() + other.mStringLength);
}

bool SafeString::operator>(const SafeString& other) const
{
    return other < (*this);
}

bool SafeString::operator <=(const SafeString& other) const
{
    return not (*this > other);
}

bool SafeString::operator>=(const SafeString& other) const
{
    return not (*this < other);
}


void SafeString::safeErase () noexcept
{
    if (mValue != nullptr)
    {
        OPENSSL_cleanse(mValue.get(), mStringLength);
        mStringLength = 0;
        mValue.reset();
    }
}

void SafeString::checkForTerminatingZero() const
{
    if (mValue != nullptr)
    {
        Expect(mValue[mStringLength] == '\0', "HEAP_CORRUPTION_DETECTED");
    }
}
