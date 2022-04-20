/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "UrlHelper.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/ByteHelper.hxx"

#include "erp/util/String.hxx"

#include <regex>


std::string UrlHelper::removeTrailingSlash (const std::string& path)
{
    std::string result = path;
    String::removeTrailingChar(result, '/');
    return result;
}

std::tuple<std::string, std::string, std::string> UrlHelper::splitTarget (const std::string& target)
{
    const auto queryStart = target.find('?');
    const auto fragmentStart = target.find('#');
    if (queryStart == std::string::npos)
        if (fragmentStart == std::string::npos)
            return { target, "", "" };
        else
            return { target.substr(0, fragmentStart), "", target.substr(fragmentStart + 1) };
    else if (fragmentStart == std::string::npos)
        return { target.substr(0, queryStart), target.substr(queryStart + 1), "" };
    else
    {
        ErpExpect(fragmentStart >= queryStart, HttpStatus::BadRequest, "invalid target: # before ?");
        return { target.substr(0, queryStart), target.substr(queryStart + 1, fragmentStart - queryStart - 1),
                 target.substr(fragmentStart + 1) };
    }
}

namespace
{
    // helper for UrlHelper::splitQuery
    std::pair<std::string, std::string> splitKeyValue (const std::string& query,
            const std::string::size_type begin, const std::string::size_type end)
    {
        const auto innerSeparator = query.find('=', begin);
        if (innerSeparator == std::string::npos || innerSeparator >= end)
        {
            // No '='. Everything to the next '&' is regarded to be the key (without value).
            return { query.substr(begin, end - begin), "" };
        }
        if (innerSeparator == end - 1)
        {
            // Found '=' but no value, ignore '='
            return { query.substr(begin, innerSeparator - begin), "" };
        }
        {

            return { query.substr(begin, innerSeparator - begin),
                     query.substr(innerSeparator + 1, end - innerSeparator - 1) };
        }
    }
}

std::vector<std::pair<std::string, std::string>> UrlHelper::splitQuery (const std::string& query)
{
    std::vector<std::pair<std::string, std::string>> keyValues;

    std::string::size_type keyValueStart = 0;
    while (keyValueStart < query.size())
    {
        const auto outerSeparator = query.find('&', keyValueStart);
        if (outerSeparator == std::string::npos)
        {
            keyValues.emplace_back(splitKeyValue(query, keyValueStart, query.size()));
            break;
        }
        else
        {
            keyValues.emplace_back(splitKeyValue(query, keyValueStart, outerSeparator));
            keyValueStart = outerSeparator + 1;
        }
    }

    return keyValues;
}

std::string UrlHelper::convertPathToRegex (const std::string& path)
{
    std::string regex = std::regex_replace(
            path,
            std::regex("\\{[^}]+\\}"),
            "([^/?]+)");
    regex = String::replaceAll(regex, "$", "\\$");
    return regex;
}

std::vector<std::string> UrlHelper::extractPathParameters (const std::string& path)
{
    std::vector<std::string> parameterNames;
    size_t parameterStart = path.find('{');
    while (parameterStart != std::string::npos)
    {
        const size_t parameterEnd = path.find('}', parameterStart);
        if (parameterEnd == std::string::npos)
            throw std::logic_error("invalid path template, opening '{' has no '}'");
        else
        {
            parameterNames.emplace_back(path.substr(parameterStart + 1, parameterEnd - parameterStart - 1));
            parameterStart = path.find('{', parameterEnd);
        }
    }
    return parameterNames;
}


std::string UrlHelper::unescapeUrl (const std::string_view& url)
{
    if (url.find_first_of("+%") == std::string::npos)
    {
        // Did not find any escape characters => nothing to do.
        return std::string(url);
    }
    else
    {
        std::string s;
        s.reserve(url.size());
        for (size_t index=0,length=url.size(); index<length; ++index)
        {
            if (url[index] == '+')
            {
                s.append(1, ' ');
            }
            else if (url[index] == '%')
            {
                ErpExpect(length>index+2, HttpStatus::BadRequest, "incomplete URL escape sequence");
                s.append(ByteHelper::fromHex(std::string_view(url.data()+index+1, 2)));
                index += 2;
            }
            else
            {
                s.append(1, url[index]);
            }
        }
        return s;
    }
}

// Implementation adapted from cpp-netlib (Boost Software License - Version 1.0 - August 17th, 2003)
std::string UrlHelper::escapeUrl (const std::string_view& str)
{
    std::string result;
    // As a heuristic reserve space for 5 escaped characters.
    result.reserve(str.size() + 5*2);

  for (size_t pos = 0; pos < str.size(); ++pos) {
    switch (str[pos]) {
      default:
        if (str[pos] >= 32 && str[pos] < 127) {
          // character does not need to be escaped
          result += str[pos];
          break;
        }
      // else pass through to next case
    [[fallthrough]];
      case '$':
      case '&':
      case '+':
      case ',':
      case '/':
      case ':':
      case ';':
      case '=':
      case '?':
      case '@':
      case '"':
      case '<':
      case '>':
      case '#':
      case '%':
      case '{':
      case '}':
      case '|':
      case '\\':
      case '^':
      case '~':
      case '[':
      case ']':
      case '`':
      case ' ':
        // the character needs to be encoded
        const auto b = static_cast<uint8_t>(str[pos]);
        result.append(1, '%');
        result.append(1, ByteHelper::nibbleToChar((b>>4) & 0x0f));
        result.append(1, ByteHelper::nibbleToChar(b & 0x0f));
        break;
    }
  };

  return result;
}


UrlHelper::UrlParts UrlHelper::parseUrl(const std::string &url)
{
    const char* SCHEME_REGEX   = "(([^:]+://))?";     // match scheme including the ://
    const char* USER_REGEX     = "(([^@/:\\s]+)@)?";  // match anything other than @ / : or whitespace before the ending @
    const char* HOST_REGEX     = "([^@/:\\s]+)";      // mandatory. match anything other than @ / : or whitespace
    const char* PORT_REGEX     = "(:([0-9]{1,5}))?";  // after the : match 1 to 5 digits
    const char* PATH_REGEX     = "(/[^#?\\s]*)?";    // after the / match anything other than # ? or whitespace
    const char* QUERY_REGEX    = "(\\?(([^?;&#=]+=[^?;&#=]+)([;|&]([^?;&#=]+=[^?;&#=]+))*))?"; // after the ? match any number of x=y pairs, separated by ; or &
    const char* FRAGMENT_REGEX = "(#([^#\\s]*))?";    // after the # match anything other than # or whitespace

    static const std::regex regExpr(std::string("")
                                    + SCHEME_REGEX + USER_REGEX
                                    + HOST_REGEX + PORT_REGEX
                                    + PATH_REGEX + QUERY_REGEX
                                    + FRAGMENT_REGEX);
    std::smatch matchResults;
    if (std::regex_match(url.cbegin(), url.cend(), matchResults, regExpr))
    {
        std::stringstream sProtocol;
        std::stringstream sHost;
        std::stringstream sRest;
        int port = 80;

        std::string scheme = matchResults.str(2);
        if (!scheme.empty())
        {
            sProtocol << scheme;
            if (String::toLower(scheme) == HTTPS_PROTOCOL)
            {
                port = 443;
            }
        }

        std::string host = matchResults.str(5);
        sHost << host;

        std::string portStr = matchResults.str(7);
        if (!portStr.empty())
        {
            port = std::stoi(portStr);
        }

        std::string path = matchResults.str(8);
        if (path.empty())
        {
            path = '/';
        }

        std::string query = matchResults.str(10);
        if (!query.empty())
        {
            sRest << "?" << query;
        }

        std::string fragment = std::string { matchResults[15].first, matchResults[15].second };
        if (!fragment.empty())
        {
            sRest << "#" << fragment;
        }

        return UrlParts(sProtocol.str(), sHost.str(), port, path, sRest.str());
    }

    throw std::runtime_error("Failed to parse: [" + url + "]");
}

UrlHelper::UrlParts::UrlParts(std::string protocol, std::string host, int port, std::string path, std::string rest)
    : mProtocol(std::move(protocol))
    , mHost(std::move(host))
    , mPort(port)
    , mPath(std::move(path))
    , mRest(std::move(rest))
{}

std::string UrlHelper::UrlParts::toString() const
{
    return mProtocol + mHost + ":" + std::to_string(mPort) + mPath + mRest;
}
