/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX

#include "EpaAccountLookupClient.hxx"
#include "shared/model/Kvnr.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

namespace exporter
{
class RuntimeConfiguration;
}

struct EpaAccount {
    enum class Code : uint8_t
    {
        allowed,
        deny,
        conflict,
        notFound,
        unknown,
        /// The account can not be determined because the relevant ePA(s) are paused in the runtime configuration.
        paused
    };
    model::Kvnr kvnr;
    std::string host;
    uint16_t port;
    Code lookupResult;
    std::set<std::string> failingHosts;
};

class IEpaAccountLookup // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
    virtual ~IEpaAccountLookup() =  default;
    virtual EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::optional<std::string>& prefix = std::nullopt) = 0;
    virtual EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList) = 0;
    virtual EpaAccount::Code checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& host, uint16_t port) = 0;
    virtual IEpaAccountLookupClient& lookupClient() = 0;
};

class EpaAccountLookup : public IEpaAccountLookup
{
public:
    explicit EpaAccountLookup(std::unique_ptr<IEpaAccountLookupClient>&& lookupClient,
                              std::shared_ptr<const exporter::RuntimeConfiguration> runtimeConfiguration = nullptr);
    explicit EpaAccountLookup(MedicationExporterServiceContext& serviceContext, const std::string& xContextId);

    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::optional<std::string>& prefix = std::nullopt) override;
    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr, const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList) override;
    EpaAccount::Code checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& host, uint16_t port) override;
    IEpaAccountLookupClient& lookupClient() override
    {
        return *mLookupClient;
    }

private:
    std::set<std::string> pausedEpas() const;

    std::unique_ptr<IEpaAccountLookupClient> mLookupClient;
    std::vector<std::tuple<std::string, uint16_t>> mEpaFqdns;
    std::shared_ptr<const exporter::RuntimeConfiguration> mRuntimeConfiguration;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EPAACCOUNTLOOKUP_HXX
