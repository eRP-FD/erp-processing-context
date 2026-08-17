/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>

class BinaryBuffer;


/**
 * @brief A read-only view over binary data (uint8_t sequences).
 *
 * BinaryView extends std::span<const std::uint8_t> with additional functionality
 * for working with binary data. It provides safe access methods with bounds checking,
 * convenient conversion to string_view, and extended slicing operations.
 *
 * This class is designed to be read-only (const) to ensure data integrity when
 * viewing existing binary data from various sources like files, network buffers,
 * or string literals. All operations are non-mutating view operations.
 */
class BinaryView : public std::span<const std::uint8_t>
{
public:
    using base_type = std::span<const std::uint8_t>;

    constexpr BinaryView() noexcept = default;

    /**
     * This automatic conversion makes it possible to pass BinaryBuffer where a BinaryView is
     * required, just like std::string_view can be constructed from std::string.
     */
    // NOLINTNEXTLINE until we have clang-tidy 19 https://github.com/llvm/llvm-project/pull/82689
    explicit(false) BinaryView(const BinaryBuffer& buffer);

    BinaryView(const void* data, size_type size);

    explicit BinaryView(const std::string_view& str);

    template<std::size_t N>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
    explicit constexpr BinaryView(const char (&s)[N]) noexcept
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      : base_type(reinterpret_cast<const value_type*>(s), N - 1)
    {
    }

    explicit constexpr BinaryView(std::span<const value_type> s) noexcept
      : base_type(s)
    {
    }

    /**
     * Returns the size of this buffer as an int, throwing an error if an overflow happens.
     * This intended to be able to use APIs such as OpenSSL without explicit gsl::narrow casts.
     */
    [[nodiscard]] int sizeAsInt() const;

    /**
     * Returns the size of this buffer as a long, throwing an error if an overflow happens.
     * This intended to be able to use APIs such as OpenSSL without explicit gsl::narrow casts.
     */
    [[nodiscard]] long sizeAsLong() const;

    [[nodiscard]] constexpr const value_type& at(size_type i) const
    {
        if (i >= size())
        {
            throw std::out_of_range("BinaryView::at: index out of range");
        }

        return (*this)[i];
    }

    [[nodiscard]] std::string_view toStringView() const noexcept
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return {reinterpret_cast<const char*>(data()), size()};
    }

    [[nodiscard]] friend constexpr bool operator==(
        const BinaryView& a,
        const BinaryView& b) noexcept
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    [[nodiscard]] constexpr BinaryView first(size_type count) const
    {
        if (count > size())
        {
            throw std::out_of_range("BinaryView::first: count out of range");
        }

        return BinaryView(base_type::first(count));
    }

    [[nodiscard]] constexpr BinaryView last(size_type count) const
    {
        if (count > size())
        {
            throw std::out_of_range("BinaryView::last: count out of range");
        }

        return BinaryView(base_type::last(count));
    }

    [[nodiscard]] constexpr BinaryView withoutPrefix(size_type count) const
    {
        if (count > size())
        {
            throw std::out_of_range("BinaryView::withoutPrefix: count out of range");
        }

        return BinaryView(subspan(count));
    }

    [[nodiscard]] constexpr BinaryView withoutSuffix(size_type count) const
    {
        if (count > size())
        {
            throw std::out_of_range("BinaryView::withoutSuffix: count out of range");
        }

        return BinaryView(base_type::first(size() - count));
    }
};
