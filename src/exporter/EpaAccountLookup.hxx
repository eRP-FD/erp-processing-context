/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX

#include "EpaAccountLookupClient.hxx"
#include "shared/model/Kvnr.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <list>
#include <string>

struct EpaAccount {
    enum class Code : uint8_t
    {
        allowed,
        deny,
        conflict,
        notFound,
        unknown
    };
    model::Kvnr kvnr;
    std::string host;
    uint16_t port;
    Code lookupResult;
};

class EpaAccountLookup
{
public:
    explicit EpaAccountLookup(std::unique_ptr<IEpaAccountLookupClient>&& lookupClient);
    explicit EpaAccountLookup(TlsCertificateVerifier tlsCertificateVerifier);

    EpaAccount lookup(const model::Kvnr& kvnr);
    EpaAccount lookup(const model::Kvnr& kvnr, const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList);
    EpaAccount::Code checkConsent(const model::Kvnr& kvnr, const std::string& host, uint16_t port);
    EpaAccountLookup& addLogAttribute(const std::string& key, const std::any& value);

private:
    std::unique_ptr<IEpaAccountLookupClient> mLookupClient;
    std::unordered_map<std::string, std::any> mLookupClientLogContext;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX