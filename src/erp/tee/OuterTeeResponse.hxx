/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEE_OUTERTEERESPONSE_HXX
#define ERP_PROCESSING_CONTEXT_TEE_OUTERTEERESPONSE_HXX

#include "shared/server/response/ServerResponse.hxx"

#include <string>


class SafeString;

class OuterTeeResponse
{
public:
    OuterTeeResponse (const std::string& a, const SafeString& aesKey);

    void convert (ServerResponse& response) const;

    std::string mEncryptedA;
};


#endif
