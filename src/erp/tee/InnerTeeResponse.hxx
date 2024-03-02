/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEE_INNERTEERESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_TEE_INNERTEERESPONSE_HXX

#include <string>


class Header;


/**
 * Representation of the inner TEE (VAU) response:
 * - '1'
 * - request id from the request
 * - header-and-response
 * separated by single spaces.
 */
class InnerTeeResponse
{
public:
    InnerTeeResponse (const std::string& requestId, const Header& header, const std::string& body);

    const std::string& getA (void) const;

private:
    const std::string mA;
};


#endif
