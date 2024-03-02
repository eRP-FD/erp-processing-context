/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGREADER_HXX
#define ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGREADER_HXX

#include "erp/common/Header.hxx"


/**
 * Helper class for reading data from a string and parse it with a boost::beast::parser to obtain
 * a request or response header and body.
 */
class BoostBeastStringReader
{
public:
    using Body = std::string;

    static std::tuple<Header,Body> parseRequest (const std::string_view& headerAndBody);
    static std::tuple<Header,Body> parseResponse (const std::string_view& headerAndBody);
};


#endif
