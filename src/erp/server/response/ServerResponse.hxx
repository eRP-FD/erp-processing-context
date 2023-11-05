/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSE_HXX

#include "fhirtools/util/Gsl.hxx"
#include "erp/common/HttpStatus.hxx"

#include <boost/noncopyable.hpp>

#include <functional>
#include <unordered_map>
#include <string>
#include <erp/common/Header.hxx>


class SslStream;

class ServerResponse : private boost::noncopyable
{
public:
    explicit ServerResponse (void);
    ServerResponse (Header header, std::string body);
    ServerResponse (const ServerResponse& other) noexcept;
    ServerResponse (ServerResponse&& other) noexcept;
    ~ServerResponse (void);

    ServerResponse& operator= (const ServerResponse& other) = delete;
    ServerResponse& operator= (ServerResponse&& other) noexcept = delete;

    void setStatus (HttpStatus status);
    void setHeader (const std::string& key, const std::string& value);
    void addWarningHeader(int code, const std::string& agent, const std::string& text);
    void removeHeader (const std::string& key);
    std::optional<std::string> getHeader (const std::string& key) const;
    const Header& getHeader (void) const;
    void setKeepAlive (bool keepAlive);

    void setBody (std::string&& body);
    void setBody (const std::string& body);
    void withoutBody (void);
    const std::string& getBody (void) const;

private:
    Header mHeader;
    std::string mBody;
    std::optional<bool> mIsKeepAliveActive;
};

#endif
