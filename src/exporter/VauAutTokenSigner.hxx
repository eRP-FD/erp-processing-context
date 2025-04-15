/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_VAUAUTTOKENSIGNER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_VAUAUTTOKENSIGNER_HXX

#include <rapidjson/document.h>
#include <string>

class HsmSession;

class VauAutTokenSigner
{
public:
    VauAutTokenSigner();

    /**
     * Using the freshness challenge, generates a JWT signed by the
     * VAU AUT key as defined in A_25165-02
     */
    std::string signAuthorizationToken(HsmSession& hsmSession, const std::string& freshness);

private:
    rapidjson::Document mPayloadDocument;
};

#endif
