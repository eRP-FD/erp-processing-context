/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_BUFFER_HXX
#define E_LIBRARY_UTIL_BUFFER_HXX

#include <vector>
#include <cstddef>
#include <string>
#include <cstdint>
#include <type_traits>
#include <iterator>


constexpr std::size_t operator""_B(unsigned long long int count)
{
    return count;
}


constexpr std::size_t operator""_KB(unsigned long long int count)
{
    return count * 1024_B;
}


/**
 * Helper to convert from megabytes to bytes.
 *
 * Uses "base 1024" units to comply with gematik requirements.
 */
constexpr std::size_t operator""_MB(unsigned long long int count)
{
    return count * 1024_KB;
}


constexpr std::size_t operator""_GB(unsigned long long int count)
{
    return count * 1024_MB;
}


namespace util
{
    using Buffer = std::vector<std::uint8_t>;

    template <typename HeadT, typename ...TailT>
    auto concatenateBuffers(HeadT&& head, TailT&&... tail);

    Buffer stringToBuffer(std::string_view string);

    std::string bufferToString(const Buffer& buffer);

    Buffer rawToBuffer(const std::uint8_t* raw, std::size_t size);

    const std::uint8_t* bufferToRaw(const Buffer& buffer);

    std::uint8_t* bufferToRaw(Buffer& buffer);
}


template <typename HeadT, typename ...TailT>
auto util::concatenateBuffers(HeadT&& head, TailT&&... tail) // NOLINT(cppcoreguidelines-missing-std-forward)
{
    // this function only works with rvalue reference parameters because
    // otherwise it would become less obvious when it is moving from params and when not
    //
    // thus, the de facto way of calling this function is:
    // util::concatenateBuffers(std::move(c1), std::move(c2), std::move(c3))
    //
    // if you need to reuse the params after calling the function,
    // then copies should be made manually by client code at the call site:
    // util::concatenateBuffers(Container{c1}, Container{c2}, Container{c3})
    //
    // named concatenateBuffers because its usages so far are only with util::Buffers,
    // so naming is suggestive towards that direction, but any type of containers will work

    static_assert(std::is_rvalue_reference_v<HeadT&&>);
    static_assert((std::is_rvalue_reference_v<TailT&&> && ...));

    // HeadT used only for the container type deduction
    // std::tuple<TailT...> works too but has the overhead of the tuple include
    //
    HeadT concatenationResult{};
    concatenationResult.reserve(head.size() + (tail.size() + ...));

    concatenationResult.insert(concatenationResult.end(),
                               std::make_move_iterator(head.begin()),
                               std::make_move_iterator(head.end())),

    (
        concatenationResult.insert(concatenationResult.end(),
                                   std::make_move_iterator(tail.begin()),
                                   std::make_move_iterator(tail.end())),
        ...
    );

    return concatenationResult;
}

#endif
