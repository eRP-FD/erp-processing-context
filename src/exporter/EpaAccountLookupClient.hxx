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

#include <any>

class MedicationExporterServiceContext;
class ClientRequest;
class HttpsClient;

class IEpaAccountLookupClient
{
public:
    virtual ~IEpaAccountLookupClient() = default;
    virtual ClientResponse sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                                       uint16_t port) = 0;
    virtual IEpaAccountLookupClient& addLogAttribute(const std::string& key, const std::any& value) = 0;
};

class EpaAccountLookupClient : public IEpaAccountLookupClient
{
public:
    EpaAccountLookupClient(MedicationExporterServiceContext& serviceContext, std::string_view consentDecisionsEndpoint,
                           std::string_view userAgent);

    ClientResponse sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                               uint16_t port) override;

    IEpaAccountLookupClient& addLogAttribute(const std::string& key, const std::any& value) override;

private:
    ClientResponse sendWithRetry(HttpsClient& client, ClientRequest& request) const;
    MedicationExporterServiceContext& mServiceContext;
    std::string mConsentDecisionsEndpoint;
    std::string mUserAgent;
    std::unordered_map<std::string, std::any> mLookupClientLogContext;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX
