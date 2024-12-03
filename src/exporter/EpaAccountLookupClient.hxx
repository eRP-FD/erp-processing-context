/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX

#include "shared/network/client/TlsCertificateVerifier.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/model/Kvnr.hxx"

class IEpaAccountLookupClient
{
public:
    virtual ~IEpaAccountLookupClient() = default;
    virtual ClientResponse sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                                       uint16_t port) = 0;
};

class EpaAccountLookupClient : public IEpaAccountLookupClient
{
public:
    EpaAccountLookupClient(uint16_t connectTimeoutSeconds, uint32_t resolveTimeoutMs,
                           std::string_view consentDecisionsEndpoint, std::string_view userAgent,
                           TlsCertificateVerifier tlsCertificateVerifier);

    ClientResponse sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                               uint16_t port) override;

private:
    uint16_t mConnectTimeoutSeconds;
    uint32_t mResolveTimeoutMs;
    std::string mConsentDecisionsEndpoint;
    std::string mUserAgent;
    TlsCertificateVerifier mTlsCertificateVerifier;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX
