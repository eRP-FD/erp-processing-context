/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookup.hxx"
#include "ExporterRequirements.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"
#include "exporter/model/ConsentDecisionsResponseType.hxx"

#include <boost/system/system_error.hpp>
#include <ranges>
#include <random>

EpaAccountLookup::EpaAccountLookup(std::unique_ptr<IEpaAccountLookupClient>&& lookupClient)
    : mLookupClient(std::move(lookupClient))
{
}

EpaAccountLookup::EpaAccountLookup(TlsCertificateVerifier tlsCertificateVerifier)
    : mLookupClient(std::make_unique<EpaAccountLookupClient>(
          Configuration::instance().getIntValue(
              ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_CONNECT_TIMEOUT_SECONDS),
          Configuration::instance().getIntValue(
              ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_RESOLVE_TIMEOUT_MILLISECONDS),
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_ENDPOINT),
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_USER_AGENT),
          std::move(tlsCertificateVerifier)))
{
}

EpaAccount EpaAccountLookup::lookup(const model::Kvnr& kvnr)
{
    static const auto epaFqdns = [] {
        auto fqdns =
            Configuration::instance().getArray(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
        std::vector<std::tuple<std::string, uint16_t>> hostPortList;
        for (const auto& fqdn : fqdns)
        {
            auto urlParts = UrlHelper::parseUrl(fqdn);
            hostPortList.emplace_back(urlParts.mHost, urlParts.mPort);
        }
        return hostPortList;
    }();
    static std::random_device rd;
    std::mt19937 gen {rd()};
    auto shuffledEpaFqdns{epaFqdns};
    std::ranges::shuffle(shuffledEpaFqdns, gen);
    return lookup(kvnr, shuffledEpaFqdns);
}

EpaAccount EpaAccountLookup::lookup(const model::Kvnr& kvnr,
                                    const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList)
{
    A_25951.start("check consent");
    bool allNotFound = true;
    for (const auto& [host, port] : epaAsHostPortList)
    {
        auto result = checkConsent(kvnr, host, port);
        switch (result)
        {
            case EpaAccount::Code::allowed:
            case EpaAccount::Code::deny:
            case EpaAccount::Code::conflict:
                return EpaAccount{.kvnr = kvnr, .host = host, .port = port, .lookupResult = result};
            case EpaAccount::Code::notFound:
                break;
            case EpaAccount::Code::unknown:
                allNotFound = false;
                break;
        }
    }
    return EpaAccount{.kvnr = kvnr,
                      .host = {},
                      .port = 0,
                      .lookupResult = allNotFound ? EpaAccount::Code::notFound : EpaAccount::Code::unknown};
    A_25951.finish();
}

EpaAccount::Code EpaAccountLookup::checkConsent(const model::Kvnr& kvnr, const std::string& host, uint16_t port)
{
    const auto response = mLookupClient->sendConsentDecisionsRequest(kvnr, host, port);
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

EpaAccountLookup& EpaAccountLookup::addLogAttribute(const std::string& key, const std::any& value)
{
    mLookupClientLogContext[key] = value;
    return *this;
}