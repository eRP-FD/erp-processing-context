/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/util/BinaryView.hxx"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>


BinaryView toBinaryView(std::string_view string);
BinaryView toBinaryView(const void* pointer, std::size_t size);

/**
 * A wrapper around a dynamic array of bytes.
 *
 * Internally, it uses an std::string for storage, as opposed to a vector of bytes, to benefit from
 * the Short String Optimization, and to make moving from and into std::string free. This comes at
 * the expense of std::string always allocating an extra null byte for us.
 */
class BinaryBuffer
{
public:
    using value_type = std::uint8_t;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    BinaryBuffer() = default;
    explicit BinaryBuffer(std::size_t size);
    BinaryBuffer(std::initializer_list<std::uint8_t> bytes);
    /** Moves the contents of the given string into a BinaryBuffer. */
    explicit BinaryBuffer(std::string string) noexcept;
    /** Copies the contents of the given view into a BinaryBuffer. */
    explicit BinaryBuffer(const BinaryView& view) noexcept;
    /** Copies the contents of the given memory range into a BinaryBuffer. */
    BinaryBuffer(const void* data, std::size_t size);

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    void clear();
    bool empty() const;

    /**
     * If necessary, removes elements at the end until the given size is reached. This never
     * performs a re-allocation. The reason for this interface instead of having a resize() method
     * is that resize() would make it easy to leak sensitive data like this:
     *
     *     SensitiveDataGuard guard{BinaryBuffer{1, 2, 3, 4}};
     *     guard->resize(10); // old bytes freed without zeroing them out
     *
     * @throw std::out_of_range if size is greater than the current size
     */
    void shrinkToSize(std::size_t size);

    std::size_t size() const;

    /**
     * Returns the size of this buffer as an int, throwing an error if an overflow happens.
     * This intended to be able to use APIs such as OpenSSL without explicit gsl::narrow casts.
     */
    int sizeAsInt() const;

    /**
     * Returns the size of this buffer as a long, throwing an error if an overflow happens.
     * This intended to be able to use APIs such as OpenSSL without explicit gsl::narrow casts.
     */
    long sizeAsLong() const;

    const std::uint8_t* data() const;
    std::uint8_t* data();

    /** Moves the contents of this BinaryBuffer into a string. */
    std::string toString() && noexcept;

    const std::string& getString() const;

    auto operator<=>(const BinaryBuffer&) const = default;

    /** Concatenates multiple BinaryBuffers, only reserving memory once in the process. */
    template<typename... Buffers>
    static BinaryBuffer concatenate(const Buffers&... buffers);

    /**
     * Return a hash value that can be used e.g. for ordered maps.
     */
    size_t hash() const;

    void append(const BinaryView& other);

private:
    friend class SensitiveDataGuard; // Only for cleanse and move-with-cleanse.

    /** Throws std::out_of_range if the size was derived from a negative value. */
    static std::size_t checkSize(std::size_t size);

    std::string mBytes;
};


BinaryBuffer stringToBinaryBuffer(std::string string);

std::string binaryBufferToString(BinaryBuffer buffer);


namespace std
{
template<>
struct hash<BinaryBuffer>
{
    size_t operator()(const BinaryBuffer& data) const noexcept
    {
        return data.hash();
    }
};
}

template<typename... Buffers>
BinaryBuffer BinaryBuffer::concatenate(const Buffers&... buffers)
{
    static_assert((std::is_same_v<BinaryBuffer, std::decay_t<Buffers>> && ...));

    BinaryBuffer concatenationResult;
    concatenationResult.mBytes.reserve((buffers.size() + ...));
    (concatenationResult.mBytes.append(buffers.mBytes), ...);
    return concatenationResult;
}
