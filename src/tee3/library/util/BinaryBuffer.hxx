/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_BINARYBUFFER_HXX
#define EPA_LIBRARY_UTIL_BINARYBUFFER_HXX

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace epa
{

class JsonReader;
class JsonWriter;


using BinaryView = std::span<const std::uint8_t>;

BinaryView toBinaryView(std::string_view string);
BinaryView toBinaryView(const void* pointer, std::size_t size);
/** Same as BinaryBuffer::sizeToInt, but for BinaryView. */
int sizeAsInt(BinaryView view);
/** Same as BinaryBuffer::sizeToLong, but for BinaryView. */
long sizeAsLong(BinaryView view);

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

    /**
     * This automatic conversion makes it possible to pass BinaryBuffer where a BinaryView is
     * required, in analogy to std::basic_string::operator std::basic_string_view.
     */
    operator BinaryView() const; // NOLINT(google-explicit-constructor, hicpp-explicit-conversions)

    auto operator<=>(const BinaryBuffer&) const = default;

    /** Concatenates multiple BinaryBuffers, only reserving memory once in the process. */
    template<typename... Buffers>
    static BinaryBuffer concatenate(const Buffers&... buffers);

    /**
     * Return a hash value that can be used e.g. for ordered maps.
     */
    size_t hash() const;

    /** Deserializes a BinaryBuffer from JSON by converting a string value from Base64. */
    static BinaryBuffer fromJson(const JsonReader& reader);

    /** Writes the BinaryBuffer to JSON as a Base64-encoded string. */
    void writeJson(JsonWriter& writer) const;

private:
    friend class SensitiveDataGuard; // Only for cleanse and move-with-cleanse.

    /** Throws std::out_of_range if the size was derived from a negative value. */
    static std::size_t checkSize(std::size_t size);

    std::string mBytes;
};


BinaryBuffer stringToBinaryBuffer(std::string string);

std::string binaryBufferToString(BinaryBuffer buffer);

} // namespace epa

namespace std
{
template<>
struct hash<epa::BinaryBuffer>
{
    size_t operator()(const epa::BinaryBuffer& data) const noexcept
    {
        return data.hash();
    }
};
}

namespace epa
{
template<typename... Buffers>
BinaryBuffer BinaryBuffer::concatenate(const Buffers&... buffers)
{
    static_assert((std::is_same_v<BinaryBuffer, std::decay_t<Buffers>> && ...));

    BinaryBuffer concatenationResult;
    concatenationResult.mBytes.reserve((buffers.size() + ...));
    (concatenationResult.mBytes.append(buffers.mBytes), ...);
    return concatenationResult;
}

} // namespace epa

#endif
