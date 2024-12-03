/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/StringEscaper.hxx"

#include <charconv>

#include "shared/util/String.hxx"

namespace epa
{

//NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
StringEscaper StringEscaper::replacingChars(std::initializer_list<std::pair<char, const char*>> map)
{
    StringEscaper result;
    for (const auto& [ch, replacement] : map)
        result.mReplacements[static_cast<unsigned char>(ch)] = replacement;
    return result;
}

std::string StringEscaper::escape(std::string str) const
{
    return String::replaceAll(
        std::move(str), [this](std::string_view view) -> String::FindNextReplacementResult {
            for (size_t pos = 0; pos < view.size(); pos++)
            {
                const char* replacement = mReplacements[static_cast<unsigned char>(view[pos])];
                if (replacement)
                    return {{view.substr(pos, 1), std::string_view(replacement)}};
            }
            return std::nullopt;
        });
}
//NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)


std::string StringEscaper::unescapeHex(char hexPrefix, std::string str)
{
    char currentChar{};

    auto findNextReplacement = [hexPrefix, &currentChar](std::string_view view) {
        String::FindNextReplacementResult result{};
        auto index = view.find(hexPrefix);
        if (index != std::string_view::npos && index + 2 < view.size())
        {
            const char* hexBegin = view.data() + index + 1;
            const char* hexEnd = hexBegin + 2;
            auto& byte = reinterpret_cast<unsigned char&>(currentChar);
            const auto [ptr, ec] = std::from_chars(hexBegin, hexEnd, byte, 16);
            // Replace the three-character percent escape (e.g. %20) with the unescaped
            // character but only if BOTH hex bytes could be parsed successfully.
            if (ec == std::errc{} && ptr == hexEnd)
                result = {{view.substr(index, 3), std::string_view(&currentChar, 1)}};
        }
        return result;
    };

    return String::replaceAll(std::move(str), findNextReplacement);
}

} // namespace epa
