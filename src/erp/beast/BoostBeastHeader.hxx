/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BOOSTBEASTHEADER_HXX
#define ERP_PROCESSING_CONTEXT_BOOSTBEASTHEADER_HXX

#include "erp/common/Header.hxx"
#include "erp/common/BoostBeastMethod.hxx"
#include "erp/util/Expect.hxx"

#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>


/**
 * A collection of functions to convert boost::beast headers into Header objects.
 */
class BoostBeastHeader
{
public:
    template<class Parser>
    static Header fromBeastRequestParser (const Parser& parser);
    template<class Parser>
    static Header fromBeastResponseParser (const Parser& parser);

    static Header::keyValueMap_t convertHeaderFields (const boost::beast::http::fields& inputFields);
};


template<class Parser>
Header BoostBeastHeader::fromBeastRequestParser (const Parser& parser)
{
    ErpExpect(parser.is_header_done(), HttpStatus::BadRequest, "Incomplete Header.");
    auto result = Header(
        fromBoostBeastVerb(parser.get().method()),
        std::string(parser.get().target()),
        parser.get().version(),
        convertHeaderFields(parser.get().base()),
        HttpStatus::Unknown); // no status for request
    result.setKeepAlive(parser.keep_alive());
    return result;
}


template<class Parser>
Header BoostBeastHeader::fromBeastResponseParser (const Parser& parser)
{
    ErpExpect(parser.is_header_done(), HttpStatus::BadRequest, "Incomplete Header.");
    auto result = Header(
        HttpMethod::UNKNOWN, // no method for response
        "", // no target for reponse
        parser.get().version(),
        convertHeaderFields(parser.get().base()),
        fromBoostBeastStatus(static_cast<uint32_t>(parser.get().result())));
    result.setKeepAlive(parser.keep_alive());
    return result;
}



#endif
