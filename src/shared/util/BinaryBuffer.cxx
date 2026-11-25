/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#include "shared/util/BinaryBuffer.hxx"
#include "shared/util/BinaryView.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Expect.hxx"

#include <limits>

#include "shared/util/Base64.hxx"


BinaryView toBinaryView(std::string_view string)
{
    return BinaryView(string);
}


BinaryView toBinaryView(const void* pointer, std::size_t size)
{
    return {pointer, size};
}


BinaryBuffer::BinaryBuffer(std::size_t size)
  : mBytes(checkSize(size), '\0')
{
}


BinaryBuffer::BinaryBuffer(std::initializer_list<std::uint8_t> bytes)
  : BinaryBuffer(std::data(bytes), std::size(bytes))
{
}


BinaryBuffer::BinaryBuffer(std::string string) noexcept
  : mBytes{std::move(string)}
{
}


BinaryBuffer::BinaryBuffer(const BinaryView& view) noexcept
  : BinaryBuffer{view.data(), view.size()}
{
}


BinaryBuffer::BinaryBuffer(const void* data, std::size_t size)
{
    Expect3(data != nullptr || size == 0, "Tried to create a BinaryBuffer from nullptr and a non-zero size.",
            std::out_of_range);
    mBytes.assign(reinterpret_cast<const char*>(data), checkSize(size));
}


BinaryBuffer::iterator BinaryBuffer::begin()
{
    return data();
}


BinaryBuffer::iterator BinaryBuffer::end()
{
    return data() + size();
}


BinaryBuffer::const_iterator BinaryBuffer::begin() const
{
    return data();
}


BinaryBuffer::const_iterator BinaryBuffer::end() const
{
    return data() + size();
}


BinaryBuffer::const_iterator BinaryBuffer::cbegin() const
{
    return data();
}


BinaryBuffer::const_iterator BinaryBuffer::cend() const
{
    return data() + size();
}


void BinaryBuffer::clear()
{
    mBytes.clear();
    mBytes.shrink_to_fit();
}


bool BinaryBuffer::empty() const
{
    return mBytes.empty();
}


void BinaryBuffer::shrinkToSize(std::size_t size)
{
    Expect(size <= mBytes.size(), "BinaryBuffer::shrinkToSize cannot increase buffer size");
    mBytes.resize(size);
}


std::size_t BinaryBuffer::size() const
{
    return mBytes.size();
}


int BinaryBuffer::sizeAsInt() const
{
    return gsl::narrow<int>(mBytes.size());
}


long BinaryBuffer::sizeAsLong() const
{
    return gsl::narrow<long>(mBytes.size());
}


const std::uint8_t* BinaryBuffer::data() const
{
    return reinterpret_cast<const std::uint8_t*>(mBytes.data());
}


std::uint8_t* BinaryBuffer::data()
{
    return reinterpret_cast<std::uint8_t*>(mBytes.data());
}


std::string BinaryBuffer::toString() && noexcept
{
    return std::move(mBytes);
}


const std::string& BinaryBuffer::getString() const
{
    return mBytes;
}

BinaryBuffer stringToBinaryBuffer(std::string string)
{
    return BinaryBuffer{std::move(string)};
}


std::string binaryBufferToString(BinaryBuffer buffer)
{
    return std::move(buffer).toString();
}

std::size_t BinaryBuffer::checkSize(std::size_t size)
{
    Expect3(size < std::numeric_limits<std::ptrdiff_t>::max(),
            "BinaryBuffer tried to use half of the available address space or more. "
            "This is most likely caused by passing a negative size.",
            std::out_of_range);
    return size;
}

void BinaryBuffer::append(const BinaryView& other)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    mBytes.append(reinterpret_cast<const char*>(other.data()), other.size());
}

size_t BinaryBuffer::hash() const
{
    return std::hash<std::string>()(mBytes);
}
