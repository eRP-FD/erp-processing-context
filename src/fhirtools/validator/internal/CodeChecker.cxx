// (C) Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
//
// non-exclusively licensed to gematik GmbH

#include "CodeChecker.hxx"

#include "fhirtools/validator/ValidationResult.hxx"

#include <rapidjson/encodings.h>
#include <iostream>

namespace fhirtools
{

namespace
{
class CheckStream
{
public:
    using Ch = rapidjson::UTF32<>::Ch;
    static bool isSpace(unsigned cp)
    {
        return cp == 0x20;
    }

    static bool isOtherWhitespace(Ch cp)
    {
        switch (cp)
        {
            case 0x0009:// Horizontal Tabulation (\t)
            case 0x000A:// Line Feed (\n)
            case 0x000B:// Vertical Tabulation (\v)
            case 0x000C:// Form Feed (\f)
            case 0x000D:// Carriage Return (\r)
            case 0x0085:// Next Line (NEL)
            case 0x00A0:// No-Break Space (NBSP)
            case 0x1680:// Ogham Space Mark
            case 0x180E:// Mongolian vowel separator
            case 0x2000:// En quad
            case 0x2001:// Em quad
            case 0x2002:// En space
            case 0x2003:// Em space
            case 0x2004:// Three-per-em space
            case 0x2005:// Four-per-em space
            case 0x2006:// Six-per-em space
            case 0x2007:// Figure space
            case 0x2008:// Punctuation space
            case 0x2009:// Thin space
            case 0x200A:// Hair space
            case 0x200B:// Zero width space
            case 0x200D:// Zero width joiner (emoji)
            case 0x2028:// Line Separator
            case 0x2029:// Paragraph Separator
            case 0x202F:// Narrow no-break space
            case 0x205F:// Medium mathematical space
            case 0x2063:// Invisible Separator
            case 0x3000:// Ideographic space
            case 0x3164:// Hangul Filler
            case 0xFEFF:// Zero width no-break space
                return true;
            default:
                return false;
        }
    }

    //NOLINTNEXTLINE(readability-identifier-naming) - name required by rapidjson
    void Put(Ch codepoint)
    {
        if (isSpace(codepoint))
        {
            ++spaceCount;
        }
        else
        {
            spaceCount = 0;
        }
        if (spaceCount > 0 && ! hadNonWhitespace)
        {
            errorMessage = "code must not start with whitespace.";
            return;
        }
        if (isOtherWhitespace(codepoint))
        {
            errorMessage = "code must not contain whitespace other than space.";
            return;
        }
        hadNonWhitespace = true;
        if (spaceCount > 1)
        {
            errorMessage = "code must not have more than one consecutive space.";
            return;
        }
    }
    bool failed() const
    {
        return ! errorMessage.empty();
    }

    bool hadNonWhitespace = false;
    size_t spaceCount = 0;
    std::string_view errorMessage;
};

class StringViewStream
{
public:
    using Ch = std::string_view::value_type;

    explicit StringViewStream(std::string_view strView)
        : mStrView{strView}
    {
    }
    Ch Take()
    {
        if (mStrView.empty())
        {
            return 0;
        }
        char c = mStrView.front();
        mStrView.remove_prefix(1);
        return c;
    }
    bool eof() const
    {
        return mStrView.empty();
    }

private:
    std::string_view mStrView;
};

}

ValidationResults CodeChecker::checkCode(std::string_view code, std::string_view elementFullPath)
{
    using Transcoder = rapidjson::Transcoder<rapidjson::UTF8<>, rapidjson::UTF32<>>;
    ValidationResults result;
    StringViewStream s{code};
    CheckStream check;
    while (!s.eof() && !check.failed()) {
        if (!Transcoder::Transcode(s, check)) {
            result.add(fhirtools::Severity::error, "invalid utf-8 in code.", std::string{elementFullPath}, nullptr);
            return result;
        }
    }
    if (check.failed())
    {
        result.add(fhirtools::Severity::error, std::string{check.errorMessage}, std::string{elementFullPath}, nullptr);
    }
    else if (check.spaceCount > 0)
    {
        result.add(fhirtools::Severity::error, "code must not end on whitespace.", std::string{elementFullPath},
                   nullptr);
    }
    return result;
}

}
