/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGWRITER_HXX
#define ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGWRITER_HXX


#include <string>

class Header;


/**
 * Helper class for serializing requests and responses into a string.
 */
class BoostBeastStringWriter
{
public:
    static std::string serializeRequest (const Header& header, const std::string_view& body);
    static std::string serializeResponse (const Header& header, const std::string_view& body);

};


#endif
