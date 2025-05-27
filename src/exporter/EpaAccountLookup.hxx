/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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

class IEpaAccountLookup // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
    virtual ~IEpaAccountLookup() =  default;
    virtual EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr) = 0;
    virtual EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList) = 0;
    virtual EpaAccount::Code checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& host, uint16_t port) = 0;
    virtual IEpaAccountLookupClient& lookupClient() = 0;
};

class EpaAccountLookup : public IEpaAccountLookup
{
public:
    explicit EpaAccountLookup(std::unique_ptr<IEpaAccountLookupClient>&& lookupClient);
    explicit EpaAccountLookup(MedicationExporterServiceContext& serviceContext);

    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr) override;
    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList) override;
    EpaAccount::Code checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& host, uint16_t port) override;
    IEpaAccountLookupClient& lookupClient() override
    {
        return *mLookupClient;
    }

private:
    std::unique_ptr<IEpaAccountLookupClient> mLookupClient;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX
