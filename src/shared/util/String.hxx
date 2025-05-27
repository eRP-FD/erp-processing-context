/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_PATH_STRING_HXX
#define FHIR_PATH_STRING_HXX

#include "fhirtools/util/Gsl.hxx"

#include <algorithm>
#include <cstdarg>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

class String
{
public:
    /**
     * Split the given string `s` at `separator`.
     */
    static std::vector<std::string> split (const std::string_view& s, char separator);

    /**
     * Split the given string `s` at `separator`.
     */
    static std::vector<std::string> split(const std::string& s, const std::string& separator);

    /**
     * Join @p sequence using operator << separated by @p separator
     */
    template<typename SequenceT>
    static std::string join(const SequenceT& sequence, std::string_view separator = ", ") requires
        requires(std::ostream& o)
    {
        o << *std::begin(sequence);
    };

    /**
     * Split the given string `s` at `separator` as long as `separator` is not contained in a
     * string that is delimited with single or double quotes.
     * This method is, among others, used for parsing a Content-Type value. Therefore quoting with
     * backslash is supported (according to RFC1341 and RFC822, quoted-string)
     */
    // Currently unused, remove if not needed in future. If it will be used this rather complex function should may be revised.
    static std::vector<std::string> splitWithStringQuoting (const std::string& s, char separator);

    /**
     * Split `s` at `separator` and return the result as a C-style array of pointers to the individual parts.
     * The returned vector contains a final nullptr element, so that its `data()` pointer can be used as nullptr
     * terminated array. Each element is trimmed and a trailing `\0` is appended.
     *
     * Returns the array of pointers and a string that acts as buffer to keep the pointed-to memory from being released.
     * The caller is responsible to not release the buffer as long as the array is in use.
     */
    static std::pair<std::vector<char*>,std::unique_ptr<char[]>> splitIntoNullTerminatedArray (const std::string& s, const std::string& separator);

    // Remark: In contrast to this version the previous version of "trim" did not treat '\v' (vertical tab) as whitespace.
    static std::string trim (const std::string& s);

    static std::string replaceAll (const std::string& in, const std::string& from, const std::string& to);

    using FindNextReplacementResult = std::optional<std::pair<std::string_view, std::string_view>>;

    /**
     * Replace all matching substrings in a string.
     *
     * findNextReplacement must be a function that takes an std::string_view and returns a
     * FindNextReplacementResult. It searches in the given view the next substring to be replaced.
     * If not found, std::nullopt must be returned. Otherwise a pair of string views has to be
     * returned: The first one is a substring (of the given view) to be replaced, and the second one
     * must be the string to be inserted instead.
     *
     * @param str  The original string.
     */
    template<class FindNextReplacement>
    [[nodiscard]] static std::string replaceAll(std::string str, FindNextReplacement findNextReplacement);

    static bool starts_with (const std::string& s, const std::string_view& head);
    static bool starts_with(const std::string_view s, const std::string_view head);

    static bool ends_with (const std::string& s, const std::string_view& tail);
    static bool ends_with(const std::string_view s, const std::string_view tail);

    [[nodiscard]] static bool contains(::std::string_view string, ::std::string_view substring);

    /* Used to strip a final character from the [in/out] argument str
    * Will only strip the token if it is the last token in the string
    */
    // Remark 1: Currently unused, remove if not needed in future.
    // Remark 2: The old version of this method did not work with single character strings equal to given trailing char.
    static void removeTrailingChar(std::string& str, const char c);

    /**
     * Remove for example enclosing double quotes from the given string `s`.
     * `head` and `tail` are independently removed if they are present at the beginning and the
     * end of `s`.
     */
    // Currently unused, remove if not needed in future:
    static std::string removeEnclosing (const std::string& head, const std::string& tail, const std::string& s);

    /**
     * Quote carriage returns and newlines in the given string like this
     * - newline is replaced with \n. If there are following characters then unquoted newline is added. A trailing
     *   newline is filtered out.
     * - carriage return is replaced with \r and then filtered out unconditionally.
     * - all other characters are added unmodified.
     */
    static std::string quoteNewlines (const std::string& in);

    /**
     * Concatenates a vector of std::string by a deliminator to a new combined std::string.
     */
    static std::string concatenateStrings(const std::vector<std::string>& strings, const std::string_view& delimiter);

    /**
     * Concatenate a couple of items, each of which can be a std::string, std::string_view or char pointer.
     * Primarily introduced because std::string and std::string_view can not be concatenated via the `+` operator.
     */
    template<typename ... Items>
    static std::string concatenateItems (Items&& ... items);

    /**
     * Concatenates the keys from a map by a deliminator to a new combined std::string.
     * `keyToString` is called to convert the map key to the string that is used for concatenation.
     */
    // Currently unused, remove if not needed in future:
    template <class M>
    static std::string concatenateKeys(const M& map, const std::string_view& delimiter,
        const std::function<std::string(const typename M::key_type&)>& keyToString =
            [](const typename M::key_type& key){ return key; });

    /**
     * Concatenates the entries from a map by an entryDeliminator
     * to a new combined std::string, where the keys are separated from values by valueDeliminator.
     */
    // Currently unused, remove if not needed in future:
    static std::string concatenateEntries(const std::map<std::string, std::string>& map,
        const char* valueDeliminator, const std::string_view& entryDeliminator);

    /**
     * A safe (as possible) way to clear the content of the given string `s`.
     * Implementation details: string contents are usually not erased, just freed. Their content is then visible to
     * whoever gets that memory next. In order to prevent that, the string is overwritten with '\0' bytes before
     * freed.
     * There are several ways to do that. An Intel SGX whitepaper suggests the use of memset_s (memset is allowed to be
     * optimized away, memset_s isn't). Sadly that function is only available in new C compilers (not C++) which we
     * apparently don't have. The documentation of memset_s suggests for C++ to use std::string::fill instead. That is what
     * is used here.
     */
    static void safeClear (std::string& s) noexcept;

    // Currently unused, remove if not needed in future:
    static size_t to_size_t (const std::string& value, size_t defaultValue);

    /**
      * Remove leading and trailing white space (trimming) and replace inner single white spaces and white space blocks
      * with a single space each.
    */
    // Currently unused, remove if not needed in future:
    static std::string normalizeWhitespace(const std::string& in);
    /**
     * Return an upper case version of the given string.
     * Note that only ASCII strings are supported. For UTF-8 (or UTF-N) we would need an external library to do
     * a proper capitalization.
     */
    static std::string toUpper (const std::string& s);
    /**
     * Return a lower case version of the given string.
     * Note that only ASCII strings are supported. For UTF-8 (or UTF-N) we would need an external library to do
     * a proper capitalization.
     */
    static std::string toLower (const std::string& s);

    /**
     * Convert the content of `s` to a boolean value. Values "TRUE", "true" (and all upper/lower case variants) and "1"
     * map to `true`. Everything else, includeing "FALSE", "false" and "0" map to `false`.
     */
    // Currently unused, remove if not needed in future:
    static bool toBool (const std::string& s);

    /**
     * Returns the length of the string in code points, not bytes. Apparently that is the way we
     * should enforce length limits.
     *
     * Reference: https://gematik.atlassian.net/servicedesk/customer/portal/2/ANFEPA-469
     */
    static std::size_t utf8Length (const std::string_view& s);

    /**
     * Truncates a string so that it is at most maxLength code points in size.
     * For any valid UTF-8 input string, the output is also valid.
     */
    // Currently unused, remove if not needed in future:
    static std::string truncateUtf8 (const std::string& s, std::size_t maxLength);

    // convert the given va_list to string using vsnprintf
    static std::string vaListToString(const char* msg, va_list args);

    static std::string toHexString(const std::string_view& input);
    static bool isHexString(const std::string& input);

    /**
     * Copy count bytes from src to dest which has destSize space. This is like memcpy_s (which is
     * not everywhere available). An exception is thrown if any pointer is null or if count is
     * greater than destSize.
     */
    static void copyMemorySafely(void* dest, size_t destSize, const void* src, size_t count);
};


template <class M>
std::string String::concatenateKeys(const M& map, const std::string_view& delimiter,
                                    const std::function<std::string(const typename M::key_type&)>& keyToString)
{
    std::ostringstream result;
    if (!map.empty())
    {
        result << keyToString(map.cbegin()->first);
        std::for_each(std::next(map.cbegin()), map.cend(),
            [&result, &delimiter, &keyToString](const auto& entry) { result << delimiter << keyToString(entry.first); });
    }
    return result.str();
}


namespace {
    [[maybe_unused]]
    void outputItems (std::ostream&)
    {
    }

    template<typename First, typename ... Rest>
    void outputItems (std::ostream& out, First&& first, Rest&& ... rest)
    {
        out << std::forward<First>(first);
        outputItems(out, std::forward<Rest>(rest)...);
    }
}

template<typename ... Items>
std::string String::concatenateItems (Items&& ... items)
{
    std::ostringstream os;
    outputItems(os, std::forward<Items>(items)...);
    return os.str();
}

template<typename SequenceT>
std::string String::join(const SequenceT& sequence, std::string_view separator) requires requires(std::ostream& o)
{
    o << *std::begin(sequence);
}
{
    std::ostringstream os;
    std::string_view sep;
    for (const auto& elem : sequence)
    {
        os << sep << elem;
        sep = separator;
    }
    return std::move(os).str();
}


template<class FindNextReplacement>
std::string String::replaceAll(std::string str, FindNextReplacement findNextReplacement)
{
    size_t tgtPos = 0, srcPos = 0, extraSpace = 0;
    for (;;)
    {
        const FindNextReplacementResult replacement =
            findNextReplacement(std::string_view(str.data() + srcPos, str.size() - srcPos));
        if (! replacement.has_value())
            break;

        const size_t removePos = gsl::narrow<size_t>(replacement->first.data() - str.data());
        const size_t removeLen = replacement->first.size();
        if (removePos + removeLen > str.size())
            throw std::runtime_error("findNextReplacement returned bad remove pos or len");

        if (removePos > srcPos)
        {
            if (tgtPos < srcPos)
                std::copy(str.data() + srcPos, str.data() + removePos, str.data() + tgtPos);
            tgtPos += removePos - srcPos;
        }
        srcPos = removePos + removeLen;

        const size_t insertLen = replacement->second.size();
        if (insertLen > 0)
        {
            if (insertLen > srcPos - tgtPos)
            {
                // We need more space. On first time getting here, only the required space is
                // allocated. But on each further time, some exponential increasing extra space is
                // added, so that we do not have to move the string in memory so often.
                extraSpace = 2 * extraSpace + insertLen - (srcPos - tgtPos);
                str.insert(srcPos, extraSpace, '\0');
                srcPos += extraSpace;
            }
            const char* insertData = replacement->second.data();
            std::copy(insertData, insertData + insertLen, str.data() + tgtPos);
            tgtPos += insertLen;
        }
    }

    if (tgtPos < srcPos)
        str.erase(tgtPos, srcPos - tgtPos);

    return str;
}


#endif
