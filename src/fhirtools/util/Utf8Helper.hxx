/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_UTIL_UTF8HELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_UTIL_UTF8HELPER_HXX

#include <string>
#include <string_view>

namespace fhirtools
{

class Utf8Helper
{
public:
    /**
     * Returns the length of the string in code points, not bytes. Apparently that is the way we
     * should enforce length limits.
     *
     * Reference: https://gematik.atlassian.net/servicedesk/customer/portal/2/ANFEPA-469
     */
    static size_t utf8Length(const std::string_view& s);

    /**
     * Truncates a string so that it is at most maxLength code points in size.
     * For any valid UTF-8 input string, the output is also valid.
     */
    // Currently unused, remove if not needed in future:
    static std::string truncateUtf8(const std::string& s, std::size_t maxLength);

    /// @param unicodeLiteral utf16 character, will be converted into utf8 and returned as std::string.
    static std::string unicodeLiteralToString(char16_t unicodeLiteral);
};

}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_UTIL_UTF8HELPER_HXX
