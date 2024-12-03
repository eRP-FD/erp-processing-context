/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_COMMON_BOOSTBEASTHEADERCONVERTER_HXX
#define EPA_LIBRARY_COMMON_BOOSTBEASTHEADERCONVERTER_HXX

#include <boost/beast/http/parser.hpp>
#include <concepts>

#include "shared/beast/BoostBeastMethod.hxx"
#include "shared/network/message/Header.hxx"


/**
 * Expect a BoostBeastRequestHeader class to have a method() method.
 * More methods must be present but this one distinguishes request headers from
 * response headers.
 */
template<typename T>
concept BoostBeastRequestHeader = requires(T a) {
    { a.method() } -> std::convertible_to<boost::beast::http::verb>;
};

/**
 * Expect a BoostBeastResponseHeader class to have a version() method.
 * More methods must be present but this one distinguishes response headers from
 * request headers.
 */
template<typename T>
concept BoostBeastResponseHeader = requires(T a) {
    { a.version() } -> std::convertible_to<int>;
};


/**
 * Convert boost::beast headers into Header objects.
 */
class BoostBeastHeaderConverter
{
public:
    template<BoostBeastRequestHeader RequestHeader>
    static Header convertRequestHeader(const RequestHeader& header);

    template<BoostBeastResponseHeader ResponseHeader>
    static Header convertResponseHeader(const ResponseHeader& header);

private:
    static Header::keyValueMap_t convertHeaderFields(const boost::beast::http::fields& inputFields);
};


template<BoostBeastRequestHeader RequestHeader>
Header BoostBeastHeaderConverter::convertRequestHeader(const RequestHeader& header)
{
    Header result(
        fromBoostBeastVerb(header.method()),
        std::string(header.target()),
        header.version(),
        convertHeaderFields(header.base()),
        HttpStatus::OK // no status for request
    );
    result.setKeepAlive(header.keep_alive());
    return result;
}


template<BoostBeastResponseHeader ResponseHeader>
Header BoostBeastHeaderConverter::convertResponseHeader(const ResponseHeader& header)
{
    Header result(
        HttpMethod::UNKNOWN, // no method for response
        std::string(),       // no target for responses
        header.version(),
        convertHeaderFields(header.base()),
        static_cast<HttpStatus>(header.result()));
    result.setKeepAlive(header.keep_alive());
    return result;
}


#endif
