/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_CLIENTINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_CLIENTINTERFACE_HXX

class ClientRequest;
class ClientResponse;


class ClientInterface
{
public:
    virtual ClientResponse send (const ClientRequest& clientRequest, const bool trustCn) = 0;
    virtual ~ClientInterface() = default;
};

#endif
