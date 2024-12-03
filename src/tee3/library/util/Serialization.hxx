/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_SERIALIZATION_HXX
#define EPA_LIBRARY_UTIL_SERIALIZATION_HXX

#include "library/util/Assert.hxx"

#include <boost/endian.hpp>
#include <cstdint>
#include <functional>

namespace epa
{

/**
 * Conversion of values to/from simple serialization format.
 */
class Serialization
{
public:
    using ByteProvider = std::function<uint8_t()>;
    using ByteConsumer = std::function<void(uint8_t)>;

    template<typename T>
    class TypedSerialization
    {
    public:
        static constexpr T read(const void* begin, const void* end);
        static T read(const ByteProvider& source);

        /**
         * Write `value` in network order (highest byte first) to the buffer (begin,end].
         * The `end` parameter is primarily used to avoid a buffer overflow, because the caller
         * has a wrong expectation of where the last byte is written.
         */
        static constexpr void write(T value, void* begin, const void* end);
        static void write(T value, const ByteConsumer& destination);

        static constexpr bool hasSpaceForT(const void* begin, const void* end);
    };

    // uint8 is missing because for a single byte there is no order to obey.
    using uint16 = TypedSerialization<uint16_t>;
    using uint32 = TypedSerialization<uint32_t>;
    using uint64 = TypedSerialization<uint64_t>;
};


template<typename T>
constexpr T Serialization::TypedSerialization<T>::read(const void* begin, const void* end)
{
    Assert(hasSpaceForT(begin, end));

    return boost::endian::endian_load<T, sizeof(T), boost::endian::order::big>(
        reinterpret_cast<const uint8_t*>(begin));
}

//NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
template<typename T>
inline T Serialization::TypedSerialization<T>::read(const ByteProvider& source)
{
    uint8_t buffer[sizeof(T)];
    for (size_t index = 0; index < sizeof(T); ++index)
        buffer[index] = source();
    return boost::endian::endian_load<T, sizeof(T), boost::endian::order::big>(buffer);
}
//NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

template<typename T>
constexpr void Serialization::TypedSerialization<T>::write(
    const T value,
    void* begin,
    const void* end)
{
    Assert(hasSpaceForT(begin, end));

    boost::endian::endian_store<T, sizeof(T), boost::endian::order::big>(
        reinterpret_cast<uint8_t*>(begin), value);
}

//NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
template<typename T>
inline void Serialization::TypedSerialization<T>::write(
    const T value,
    const ByteConsumer& destination)
{
    uint8_t buffer[sizeof(T)];
    boost::endian::endian_store<T, sizeof(T), boost::endian::order::big>(buffer, value);
    for (size_t index = 0; index < sizeof(T); ++index)
        destination(buffer[index]);
}
//NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)


template<typename T>
constexpr bool Serialization::TypedSerialization<T>::hasSpaceForT(
    const void* begin,
    const void* end)
{
    Assert(begin != nullptr);
    Assert(end != nullptr);
    const auto available = static_cast<size_t>(std::distance(
        reinterpret_cast<const uint8_t*>(begin), reinterpret_cast<const uint8_t*>(end)));
    return available >= sizeof(T);
}

} // namespace epa

#endif
