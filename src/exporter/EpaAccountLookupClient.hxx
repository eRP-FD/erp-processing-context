/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX

#include "exporter/BdeMessage.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/network/client/TlsCertificateVerifier.hxx"
#include "shared/network/client/response/ClientResponse.hxx"

#include <any>

class MedicationExporterServiceContext;
class ClientRequest;
class HttpsClient;

class IEpaAccountLookupClient
{
public:
    virtual ~IEpaAccountLookupClient() = default;
    virtual ClientResponse sendConsentDecisionsRequest(const std::string& xRequestId, const model::Kvnr& kvnr,
                                                       const std::string& host, uint16_t port) = 0;
    virtual IEpaAccountLookupClient& addLogAttribute(const BDEMessage::Data& bdeData) = 0;
};

class EpaAccountLookupClient : public IEpaAccountLookupClient
{
public:
    EpaAccountLookupClient(MedicationExporterServiceContext& serviceContext, std::string_view consentDecisionsEndpoint,
                           const std::string& xContextId);

    ClientResponse sendConsentDecisionsRequest(const std::string& xRequestId, const model::Kvnr& kvnr,
                                               const std::string& host, uint16_t port) override;

    IEpaAccountLookupClient& addLogAttribute(const BDEMessage::Data& bdeData) override;

private:
    ClientResponse sendWithRetry(HttpsClient& client, ClientRequest& request) const;
    MedicationExporterServiceContext& mServiceContext;
    std::string mConsentDecisionsEndpoint;
    BDEMessage::Data mBdeData;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUPCLIENT_HXX
