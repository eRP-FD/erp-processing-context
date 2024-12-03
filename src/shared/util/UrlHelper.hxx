/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_URLHELPER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_URLHELPER_HXX

#include <string>
#include <unordered_map>
#include <vector>


class UrlHelper
{
public:
    static constexpr const char* HTTPS_PROTOCOL = "https://";
    static constexpr const char* HTTP_PROTOCOL = "http://";
    struct UrlParts
    {
        std::string mProtocol;
        std::string mHost;
        int mPort;
        std::string mPath;
        std::string mRest;

        UrlParts(std::string protocol, std::string host, int port, std::string path, std::string rest);
        bool isHttpsProtocol() const;
        bool isHttpProtocol() const;

        std::string toString() const;
    };


    /// @return path with one trailing / removed
    static std::string removeTrailingSlash (const std::string& path);

    /// @return {target, query, fragment} from "target?[query]#[fragment]"
    static std::tuple<std::string, std::string, std::string> splitTarget (const std::string& target);

    /// @return <key,value> pairs for queries from query part in URL, e.g. "p1=A&p2=B"
    static std::vector<std::pair<std::string, std::string>> splitQuery (const std::string& query);

    /// @return "([^/?]+)" for "\{[^}]\}", and replace each $ by \$
    static std::string convertPathToRegex (const std::string& path);

    /// @return {"a", "bb"} for "{a}{bb}"
    static std::vector<std::string> extractPathParameters (const std::string& path);

    /**
     * Replace each `%xx` with a single character with hex value xx.
     * Replace each '+' with a single space.
     */
    static std::string unescapeUrl (const std::string_view& url);

    /**
     * Replace any character below 0x20 and above 0x79 by `%xx`.
     * Replace any space with `%20`.
     * Subject to change.
     */
    static std::string escapeUrl (const std::string_view& url);

    /**
     * Parses url into an UrlParts object.
     *
     * @param url input url
     * @throws std::runtime_error if url cannot be parsed
     * @return url parts object
     */
    static UrlParts parseUrl(const std::string& url);
};

#endif
