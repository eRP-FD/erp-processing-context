/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_STRINGESCAPER_HXX
#define EPA_LIBRARY_UTIL_STRINGESCAPER_HXX

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility> // for std::pair

namespace epa
{

/**
 * Helper class that wraps the generic String::replaceAll algorithm for the special case where we
 * can use a char-based lookup table (std::array with 256 entries).
 */
class StringEscaper
{
public:
    /**
     * Creates an instance where every character in the list is mapped to a string constant.
     * This constructor does not make copies of the given strings! They are expected to be string
     * literals.
     */
    static StringEscaper replacingChars(std::initializer_list<std::pair<char, const char*>> map);

    /**
     * Creates an instance where every character in the given list is escaped as HexPrefix followed
     * by two upper-case hexadecimal digits.
     */
    //NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
    template<char HexPrefix>
    static StringEscaper escapingCharsAsHex(std::string_view chars)
    {
        StringEscaper result;
        for (char uc : chars)
            result.mReplacements[uc] = hexTable<HexPrefix>[uc].data();
        return result;
    }

    /** Same as escapingCharsAsHex, but for all 256 possible character values. */
    template<char HexPrefix>
    static StringEscaper allToHex()
    {
        StringEscaper result;
        for (unsigned u = 0; u < 256; ++u)
            result.mReplacements[u] = hexTable<HexPrefix>[u].data();
        return result;
    }

    /** Returns a copy of this instances which excludes some characters from escaping. */
    StringEscaper except(std::string_view ignore) const
    {
        StringEscaper escaper = *this;
        for (char uc : ignore)
            escaper.mReplacements[static_cast<unsigned char>(uc)] = nullptr;
        return escaper;
    }
    //NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

    std::string escape(std::string str) const;

    static std::string unescapeHex(char hexPrefix, std::string str);

private:
    /** Nibble-to-hex conversion string. Use upper-case because S3Canonical* requires it. */
    static constexpr const char* hexChars = "0123456789ABCDEF";

    /** Templated constant for a table from uint8_t to its escaped string constant. */
    //NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
    template<char HexPrefix>
    static constexpr auto hexTable = []() -> auto {
        std::array<std::array<char, 4>, 256> table{};
        for (size_t i = 0; i < table.size(); ++i)
        {
            table[i][0] = HexPrefix;
            table[i][1] = hexChars[i >> 4];
            table[i][2] = hexChars[i & 15];
            table[i][3] = '\0';
        }
        return table;
    }();
    //NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

    std::array<const char*, 256> mReplacements{{nullptr}};
};

} // namespace epa

#endif
