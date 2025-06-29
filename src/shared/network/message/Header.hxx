/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_COMMON_HEADER_HXX
#define E_LIBRARY_COMMON_HEADER_HXX

#include "shared/model/ProfileType.hxx"
#include "shared/network/message/HttpMethod.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/network/message/MimeType.hxx"
#include "shared/util/String.hxx"

#include <optional>
#include <string>
#include <unordered_map>


struct StringHashCaseInsensitive
{
    size_t operator() (const std::string& s) const;
};

struct StringComparatorCaseInsensitive
{
    bool operator() (const std::string& a, const std::string& b) const;
};



class Header
{
public:
    using keyValueMap_t =
        std::unordered_map<std::string, std::string, StringHashCaseInsensitive, StringComparatorCaseInsensitive>;

    Header (void);
    explicit Header (HttpStatus statusCode);

    Header(
        HttpMethod method,
        std::string&& target,
        unsigned int version,
        keyValueMap_t&& header,
        HttpStatus statusCode);

    HttpMethod method (void) const;
    void setMethod (HttpMethod method);

    const std::string& target (void) const;
    void setTarget (const std::string& target);

    static constexpr unsigned int Version_1_0 = 10;
    static constexpr unsigned int Version_1_1 = 11;
    static constexpr std::string_view ExternalInterface_TI = "TI"; // Telematik-Infrastructure (LEI access)
    static constexpr std::string_view ExternalInterface_INTERNET = "INTERNET"; // Insurants

    unsigned int version (void) const;
    void setVersion (unsigned int version);

    std::optional<const std::string> header (const std::string& key) const;
    bool hasHeader (const std::string& key) const;
    size_t contentLength (void) const;
    std::optional<const std::string> contentType (void) const;

    HttpStatus status (void) const;
    void setStatus (HttpStatus status);
    bool keepAlive (void) const;
    void setKeepAlive(bool keepAlive);

    /// @return MimeType from Accept header field, if present
    std::optional<AcceptMimeType> getAcceptMimeType(std::string_view mimeType) const;


    const keyValueMap_t& headers (void) const;

    /**
     * Deprecated. Use BoostBeastStringWriter instead.
     *
     * Serialize the header fields (i.e. no first line, no trailing empty line) into a single string.
     * Individual lines are delimited with Carriage-Return and Newline pairs.
     *
     * Use this method only for specific use cases like the epa additional headers.
     * For more generic cases use BoostBeastHeader or the BoostBeastString(Reader|Writer) classes.
     */
    std::string serializeFields (void) const;

    /**
     * Add the given `fields` to the header fields of the called `Header` object.
     */
    void addHeaderFields (const std::unordered_map<std::string, std::string>& fields);
    void addHeaderField (const std::string& key, const std::string& value);

    void removeHeaderField (const std::string& key);

    /**
     * Check Invariants (as per https://tools.ietf.org/html/rfc7230#section-3.3.2).
     * - Content-Length and Transfer-Encoding can not be used at the same time.
     * - Content-Length OR Transfer-Encoding is not allowed for status codes
     *   - 204
     *   - 1xx
     *   - 2xx for CONNECT requests
     * Throws a std::logic_error if any of the invariants (see class description) is violated (CONNECT is not checked).
     */
    void checkInvariants (void) const;

    void setContentLength (size_t contentLength);

    static std::string profileVersionHeader(model::ProfileType profileType);

    static const std::string Accept;
    static const std::string AcceptLanguage;
    static const std::string Authorization;
    static const std::string Connection;
    static const std::string ContentEncoding;
    static const std::string ContentLength;
    static const std::string ContentType;
    static const std::string ContextStatus;
    static const std::string ExternalInterface;
    static const std::string Host;
    static const std::string KeepAlive;
    static const std::string Location;
    static const std::string Server;
    static const std::string Session;
    static const std::string Signature;
    static const std::string SigningCertificate;
    static const std::string Timestamp;
    static const std::string TransferEncoding;
    static const std::string UserAgent;
    static const std::string WWWAuthenticate;
    static const std::string Warning;
    static const std::string XAccessCode;
    static const std::string XRequestId;

    // header fields in the outer response that are evaluated by the proxy
    static const std::string VAUErrorCode;
    static const std::string InnerRequestRole;
    static const std::string InnerRequestOperation;
    static const std::string InnerRequestFlowtype;
    static const std::string InnerRequestClientId;
    static const std::string InnerRequestLeips;
    static const std::string InnerResponseCode;
    static const std::string BackendDurationMs;
    static const std::string MvoNumber;
    static const std::string ANR;
    static const std::string ZANR;
    static const std::string PrescriptionId;
    static const std::string DigaRedeemCode;
    static const std::string GematikWorkflowProfil;
    static const std::string GematikPatientenrechnung;
    static const std::string KbvVerordnungsdaten;
    static const std::string DavAbgabedaten;

    struct Tee3 {
        static const std::string VauCid;
        static const std::string XUserAgent;
        static const std::string XInsurantId;
        static const std::string VauNP;
        static const std::string VauNonPuTracing;
    };



    // Values for selected header fiels.
    static const std::string ConnectionClose;
    static const std::string ConnectionKeepAlive;

    // EPA Headers
    struct Epa {
        static const std::string ClusterAddress;
        static const std::string PodAddress;
    };


private:
    void parseAcceptField(std::string_view acceptField);
    void setContentLengthZeroMethodUnknown();

    HttpMethod mMethod;
    std::string mTarget;
    unsigned int mVersion;
    keyValueMap_t mHeader;
    HttpStatus mStatusCode;
    std::vector<AcceptMimeType> mAcceptMimeTypes;
};

#endif
