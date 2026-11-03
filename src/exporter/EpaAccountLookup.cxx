/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookup.hxx"
#include "ExporterRequirements.hxx"
#include "exporter/model/ConsentDecisionsResponseType.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "exporter/util/RuntimeConfiguration.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"

#include <boost/system/system_error.hpp>
#include <random>
#include <ranges>

namespace
{
std::vector<std::tuple<std::string, uint16_t>> epaFqdns()
{
    const auto& fqdns = Configuration::instance().epaFQDNs();
    std::vector<std::tuple<std::string, uint16_t>> hostPortList;
    hostPortList.reserve(fqdns.size());
    for (const auto& fqdn : fqdns)
    {
        hostPortList.emplace_back(fqdn.hostName, fqdn.port);
    }
    return hostPortList;
};
}

EpaAccountLookup::EpaAccountLookup(std::unique_ptr<IEpaAccountLookupClient>&& lookupClient,
                                   std::shared_ptr<const exporter::RuntimeConfiguration> runtimeConfiguration)
    : mLookupClient(std::move(lookupClient))
    , mEpaFqdns{epaFqdns()}
    , mRuntimeConfiguration(std::move(runtimeConfiguration))
{
}

EpaAccountLookup::EpaAccountLookup(MedicationExporterServiceContext& serviceContext, const std::string& xContextId)
    : EpaAccountLookup(std::make_unique<EpaAccountLookupClient>(
          serviceContext,
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENDPOINT),
          xContextId),
      serviceContext.getRuntimeConfiguration())
{
}

EpaAccount EpaAccountLookup::lookup(const std::string& xRequestId, const model::Kvnr& kvnr,
                                    const std::optional<std::string>& prefix /* = std::nullopt */)
{
    static std::random_device rd;
    std::mt19937 gen{rd()};
    std::ranges::shuffle(mEpaFqdns, gen);
    if (prefix.has_value())
    {
        size_t idx = 0;
        for (auto& entry : mEpaFqdns)
        {
            if (std::get<0>(entry).starts_with(prefix.value() + "."))
            {
                const auto& [host, port] = entry;
                if (pausedEpas().contains(host))
                {
                    // the cached ePA is paused, no need to ask any ePA
                    return EpaAccount{.kvnr = kvnr,
                                      .host = host,
                                      .port = port,
                                      .lookupResult = EpaAccount::Code::paused,
                                      .failingHosts = {}};
                }
                std::swap(mEpaFqdns[0], mEpaFqdns[idx]);
                break;
            }
            idx++;
        }
    }
    return lookup(xRequestId, kvnr, mEpaFqdns);
}

EpaAccount EpaAccountLookup::lookup(const std::string& xRequestId, const model::Kvnr& kvnr,
                                    const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList)
{
    A_25951.start("check consent");
    bool allNotFound = true;
    bool anySkipped = false;
    std::set<std::string> failingHosts;
    const auto paused = pausedEpas();
    for (const auto& [host, port] : epaAsHostPortList)
    {
        if (paused.contains(host))
        {
            anySkipped = true;
            continue;
        }
        auto result = checkConsent(xRequestId, kvnr, host, port);
        switch (result)
        {
            case EpaAccount::Code::allowed:
            case EpaAccount::Code::deny:
            case EpaAccount::Code::conflict:
                return EpaAccount{.kvnr = kvnr,
                                  .host = host,
                                  .port = port,
                                  .lookupResult = result,
                                  .failingHosts = failingHosts};
            case EpaAccount::Code::notFound:
                break;
            case EpaAccount::Code::unknown:
                failingHosts.emplace(host);
                allNotFound = false;
                break;
            case EpaAccount::Code::paused:// never returned by checkConsent
                break;
        }
    }
    A_25951.finish();
    auto lookupResult = EpaAccount::Code::unknown;
    if (allNotFound)
    {
        // the account may exist at an ePA that was not asked because it is paused
        lookupResult = anySkipped ? EpaAccount::Code::paused : EpaAccount::Code::notFound;
    }
    return EpaAccount{.kvnr = kvnr, .host = {}, .port = 0, .lookupResult = lookupResult, .failingHosts = failingHosts};
}

std::set<std::string> EpaAccountLookup::pausedEpas() const
{
    if (! mRuntimeConfiguration)
    {
        return {};
    }
    return exporter::RuntimeConfigurationGetter{mRuntimeConfiguration}.getPausedEpas();
}

EpaAccount::Code EpaAccountLookup::checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr,
                                                const std::string& host, uint16_t port)
{
    const auto response = mLookupClient->sendConsentDecisionsRequest(xRequestId, kvnr, host, port);
    if (response.getHeader().status() == HttpStatus::OK)
    {
        model::ConsentDecisionsResponseType consentDecisionsResponse{response.getBody()};
        const auto erpSubmissionFunctionId = Configuration::instance().getStringValue(
            ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ERP_SUBMISSION_FUNCTION_ID);
        auto decision = consentDecisionsResponse.getDecisionFor(erpSubmissionFunctionId);
        Expect(decision, "could not get decision for " + erpSubmissionFunctionId + " in response from " + host);
        switch (*decision)
        {
            case model::ConsentDecisionsResponseType::Decision::permit:
                return EpaAccount::Code::allowed;
            case model::ConsentDecisionsResponseType::Decision::deny:
                return EpaAccount::Code::deny;
        }
        Fail("invalid decision value for " + erpSubmissionFunctionId + " in response from " + host);
    }
    else if (response.getHeader().status() == HttpStatus::Conflict)
    {
        return EpaAccount::Code::conflict;
    }
    else if (response.getHeader().status() == HttpStatus::NotFound)
    {
        return EpaAccount::Code::notFound;
    }
    return EpaAccount::Code::unknown;
}
