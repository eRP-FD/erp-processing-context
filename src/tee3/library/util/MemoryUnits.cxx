/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/MemoryUnits.hxx"

namespace epa
{

std::string MemoryUnits::sizeToString(std::size_t bytes)
{
    // Formats 1234 as "1.23" and 19 as "0.02".
    const auto dividedBy1000 = [](auto count) {
        // 1234 -> "123" and 19 -> "2"
        std::string result = std::to_string((count + 5) / 10);
        // "123" -> "123" but "2" -> "002".
        if (result.length() < 3)
            result.insert(0, 3 - result.length(), '0');
        // "123" -> "1.23" and "002" -> "0.02".
        result.insert(result.size() - 2, 1, '.');
        return result;
    };

    if (bytes < 1000)
        return std::to_string(bytes) + " bytes";

    if (bytes < 1'000'000)
        return dividedBy1000(bytes) + " KB";

    return dividedBy1000(bytes / 1000) + " MB";
}

} // namespace epa
