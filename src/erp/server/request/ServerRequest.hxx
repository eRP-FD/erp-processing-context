#ifndef ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUEST_HXX

#include "erp/common/Header.hxx"
#include "erp/crypto/Jwt.hxx"

#include <string>
#include <optional>

class JWT;

class ServerRequest
{
public:
    explicit ServerRequest (Header&& header);

    const Header& header (void) const;
    Header& header (void);
    void setHeader (Header&& header);
    void setMethod(HttpMethod method);

    void setBody (std::string&& body);
    const std::string& getBody (void) const;

    /**
    * Path parameters are defined as a specific part in the URL path, e.g. when
    *     https://domain/resource/123
    * matches
    *     https://domain/resource/{id}
    * then
    *     id=123
    * is a path paramater
     */
    size_t getPathParameterCount (void) const;
    std::optional<std::string> getPathParameter (const std::string& parameterName) const;
    const std::unordered_map<std::string, std::string>& getPathParameters (void) const;
    void setPathParameters (const std::vector<std::string>& keys, const std::vector<std::string>& values);

    /**
     * Query parameters are defined by key=value pairs after the '?' and are separated by '&', e.g.
     * https://domain/path?key1=value2&key2=value2
     * Query parameters can occur multiple times (for the same key) and their order can be important.
     */
    using QueryParametersType = std::vector<std::pair<std::string,std::string>>;
    void setQueryParameters (QueryParametersType&& parameters);
    const QueryParametersType& getQueryParameters (void) const;
    /**
     * This is a convenience method in case a query parameter is expected at most one times.
     * If the parameter is contained more than once in the url, an exception is thrown (ErpException(HttpStatus::BadRequest))
     */
    std::optional<std::string> getQueryParameter (const std::string& key) const;

    void setFragment (std::string&& fragment);
    const std::string& getFragment (void) const;

    void setAccessToken(JWT&& jwt);
    const JWT& getAccessToken(void) const; 

private:
    Header mHeader;
    std::string mBody;
    std::unordered_map<std::string, std::string> mPathParameters;
    QueryParametersType mQueryParameters;
    std::string mFragment;
    JWT mAccessToken;
};


#endif
