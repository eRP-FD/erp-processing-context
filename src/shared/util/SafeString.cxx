/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/SafeString.hxx"
#include "shared/util/Expect.hxx"

#include <openssl/crypto.h>
#include <algorithm>
#include <span>

SafeString::SafeString ()
    : SafeString(static_cast<size_t>(0))
{
}

SafeString::SafeString(const SafeString::NoZeroFillTag&, size_t size)
    : mValue(std::make_unique<char[]>(size + 1))
    , mStringLength(size)
{
    Expect3(size < std::numeric_limits<size_t>::max(), "size out of range", std::out_of_range);
    setTerminatingZero();
}

SafeString::SafeString(const SafeString& other)
    : SafeString(SafeString::no_zero_fill, other.size())
{
    const std::string_view otherValueView{other};
    std::ranges::copy(otherValueView, mValue.get());
}

SafeString::SafeString (const size_t size)
    : SafeString(no_zero_fill, size)
{
    memset(mValue.get(), 0x00, mStringLength);
}

SafeString::SafeString(char* value, size_t size)
    : SafeString(no_zero_fill, size)
{
    std::ranges::copy(std::string_view{value, size}, mValue.get());
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
    std::ranges::copy(std::string_view{value}, mValue.get());
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
    std::ranges::copy(std::string_view{value, size}, mValue.get());
    mStringLength = size;
    setTerminatingZero();
    OPENSSL_cleanse(value, size);
}

SafeString::~SafeString ()
{
    safeErase();
}

SafeString::operator const char* () const
{
    return c_str();
}

SafeString::operator const unsigned char* () const
{
    checkForTerminatingZero();
    return reinterpret_cast<const unsigned char*>(mValue.get());
}

SafeString::operator std::string_view () const
{
    checkForTerminatingZero();
    return { mValue.get(), mStringLength };
}

SafeString::operator std::basic_string_view<std::byte>() const
{
    checkForTerminatingZero();
    return {reinterpret_cast<const std::byte*>(mValue.get()), mStringLength };
}

SafeString::operator gsl::span<const char> () const
{
    checkForTerminatingZero();
    return { mValue.get(), mStringLength };
}

SafeString::operator char* ()
{
    return c_str();
}

SafeString::operator std::byte*()
{
    checkForTerminatingZero();
    return reinterpret_cast<std::byte*>(mValue.get());
}

char* SafeString::c_str()
{
    checkForTerminatingZero();
    return mValue.get();
}

char* SafeString::begin()
{
    return c_str();
}

const char* SafeString::c_str() const
{
    checkForTerminatingZero();
    return mValue.get();
}

size_t SafeString::size () const
{
    return mStringLength;
}

bool SafeString::operator==(const SafeString& other) const
{
    return mStringLength == other.mStringLength
        && std::ranges::equal(std::string_view{*this}, std::string_view{other});
}

bool SafeString::operator!=(const SafeString& other) const
{
    return not (*this == other);
}

bool SafeString::operator < (const SafeString& other) const
{
    return std::ranges::lexicographical_compare(std::string_view{*this}, std::string_view{other});
}

bool SafeString::operator>(const SafeString& other) const
{
    return other < *this;
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

void SafeString::resize(size_t newSize)
{
    if (newSize == mStringLength)
    {
        return;
    }
    SafeString tmp{newSize};
    std::copy_n(begin(), std::min(size(), newSize), tmp.begin());
    std::swap(*this, tmp);
}

void SafeString::checkForTerminatingZero() const
{
    if (mValue != nullptr)
    {
        Expect(std::span(mValue.get(), mStringLength + 1).back() == '\0', "HEAP_CORRUPTION_DETECTED");
    }
}

void SafeString::setTerminatingZero()
{
    if (mValue != nullptr)
    {
        std::span(mValue.get(), mStringLength + 1).back() = '\0';
    }
}
