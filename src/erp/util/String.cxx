/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/util/String.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/GLog.hxx"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <openssl/crypto.h>
#include <iomanip>
#include <regex>

#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/util/Utf8Helper.hxx"
#include "fhirtools/util/VaListHelper.hxx"

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

std::vector<std::string> String::split (const std::string_view& s, char separator)
{
    std::vector<std::string> parts;
    boost::split(
        parts,
        s,
        [separator](char c) { return c == separator; });
    return parts;
}

std::vector<std::string> String::split(const std::string& s, const std::string& separator)
{
    // Please note:
    // In a real generic method "skipEmptyParts" should be an input parameter.
    // Here it is not used as the String class is not a generic class but the class only
    // includes what is really needed in the project.
    // Nevertheless the use of the parameter has been kept in case it is needed in the future.
    bool skipEmptyParts = false;

    std::vector<std::string> tokens;

    for (size_t start = 0, end = 0; start < s.length(); start = end + separator.length())
    {
        size_t position = s.find(separator, start);
        end = position != std::string::npos ? position : s.length();

        std::string token = s.substr(start, end - start);
        if (!skipEmptyParts || !token.empty())
        {
            tokens.emplace_back(token);
        }
    }

    if (!skipEmptyParts && (s.empty() || ends_with(s, separator)))
    {
        tokens.emplace_back("");
    }

    return tokens;
}

std::vector<std::string> String::splitWithStringQuoting (const std::string& s, char separator)
{
    std::vector<std::string> parts;
    const size_t length = s.size();
    size_t index = 0;
    std::stringstream part;
    bool expectingClosingSingleQuote = false;
    bool expectingClosingDoubleQuote = false;

    while (index < length)
    {
        const char c = s[index++];
        if (c == separator)
        {
            if (expectingClosingSingleQuote || expectingClosingDoubleQuote)
                part << c;
            else
            {
                parts.push_back(trim(part.str()));
                part.str("");
            }
        }
        else
            switch (c)
            {
                case '\'':
                    part << c;
                    expectingClosingSingleQuote = !expectingClosingSingleQuote && !expectingClosingDoubleQuote;
                    break;
                case '\"':
                    part << c;
                    expectingClosingDoubleQuote = !expectingClosingSingleQuote && !expectingClosingDoubleQuote;
                    break;
                case '\\':
                    if (index < length)
                        part << s[index++];
                    // else ignore a trailing backslash.
                    break;

                default:
                    part << c;
                    break;
            }
    }
    parts.push_back(trim(part.str()));
    return parts;
}


std::pair<std::vector<char*>,std::unique_ptr<char[]>> String::splitIntoNullTerminatedArray (const std::string& s, const std::string& separator)
{
    const auto items = String::split(s, separator);
    std::vector<char*> array;
    array.reserve(items.size());

    auto buffer = std::make_unique<char[]>(s.size()+1);
    char* p = buffer.get();
    for (const auto& item : items)
    {
        const auto trimmedItem = String::trim(item);
        array.push_back(p);
        std::copy(trimmedItem.begin(), trimmedItem.end(), p);
        p += trimmedItem.size();
        *p++ = '\0';
    }

    // Mark the end of the list.
    array.push_back(nullptr);

    return {array, std::move(buffer)};
}


std::string String::trim (const std::string& s)
{
    return boost::trim_copy(s);
}


std::string String::replaceAll (const std::string& in, const std::string& from, const std::string& to)
{
    return boost::algorithm::replace_all_copy(in, from, to);
}


bool String::starts_with (const std::string& s, const std::string_view& head)
{
    return boost::starts_with(s, head);
}

bool String::starts_with(const std::string_view s, const std::string_view head)
{
    return boost::starts_with(s, head);
}

bool String::ends_with (const std::string& s, const std::string_view& tail)
{
    return boost::ends_with(s, tail);
}

bool String::ends_with(const std::string_view s, const std::string_view tail)
{
    return boost::ends_with(s, tail);
}

bool String::contains(::std::string_view string, ::std::string_view substring)
{
    return ::boost::contains(string, substring);
}

std::string String::quoteNewlines (const std::string& in)
{
    std::ostringstream out;
    for(auto iter = in.cbegin(); iter != in.cend(); iter++)
    {
        switch (*iter)
        {
            case '\r':
                out << "\\r";
                break;
            case '\n':
                out << "\\n";
                if(std::next(iter) != in.cend())
                    out << "\n";
                break;
            default:
                out << *iter;
                break;
        }
    }
    return out.str();
}


void String::removeTrailingChar(std::string& str, const char c)
{
    if(!str.empty() && str.back() == c)
        str.pop_back();
}


std::string String::removeEnclosing (const std::string& head, const std::string& tail, const std::string& s)
{
    std::size_t startPos = starts_with(s, head) ? head.size() : 0;
    std::size_t endPos = ends_with(s, tail) ? s.size() - tail.size() : s.size();

    if(startPos >= endPos)
        return "";
    else
        return s.substr(startPos, endPos - startPos);
}


std::string String::concatenateStrings(const std::vector<std::string>& strings, const std::string_view& delimiter)
{
    return boost::join(strings, delimiter);
}


std::string String::concatenateEntries(const std::map<std::string, std::string>& map,
                                       const char* valueDeliminator, const std::string_view& entryDeliminator)
{
    std::ostringstream result;
    if(!map.empty())
    {
        const auto& elem = *map.cbegin();
        result << elem.first << valueDeliminator << elem.second;
        std::for_each(
            std::next(map.cbegin()), map.cend(),
            [&result, &valueDeliminator, &entryDeliminator](const auto& entry)
            {
                result << entryDeliminator << entry.first << valueDeliminator << entry.second;
            });
    }
    return result.str();
}


void String::safeClear (std::string& s) noexcept
{
    OPENSSL_cleanse(s.data(), s.size());
    s.clear();
}


size_t String::to_size_t (const std::string& value, const size_t defaultValue)
{
    try
    {
        return stoul(value);
    }
    catch(...)
    {
        TLOG(ERROR) << "conversion of " << value << " to size_t failed, returning default " << defaultValue;
    }
    return defaultValue;
}


std::string String::normalizeWhitespace (const std::string& in)
{
    std::ostringstream out;
    bool first = true;
    bool skippingWhitespace = false;
    for (const char c : in)
    {
        switch (c)
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                skippingWhitespace = true;
                break;

            default:
                if (first)
                    first = false;
                else if (skippingWhitespace)
                {
                    out << ' ';
                    skippingWhitespace = false;
                }
                out << c;
                break;
        }
    }
    return out.str();
}


std::string String::toUpper (const std::string& s)
{
    return boost::to_upper_copy(s);
}


std::string String::toLower (const std::string& s)
{
    return boost::to_lower_copy(s);
}


bool String::toBool (const std::string& s)
{
    const std::string lower = toLower(s);
    return lower=="true" || lower=="1";
}


std::size_t String::utf8Length (const std::string_view& s)
{
    return fhirtools::Utf8Helper::utf8Length(s);
}


std::string String::truncateUtf8 (const std::string& s, std::size_t maxLength)
{
    return fhirtools::Utf8Helper::truncateUtf8(s, maxLength);
}

std::string String::vaListToString(const char* msg, va_list args)
{
    return fhirtools::VaListHelper::vaListToString(msg, args);
}

std::string String::toHexString(const std::string_view& input)
{
    std::stringstream sstr;
    sstr << std::hex << std::setfill('0');
    for (const auto& byte : input)
    {
        sstr << std::setw(2) << (static_cast<int>(byte) & 0xff);
    }
    return sstr.str();
}
