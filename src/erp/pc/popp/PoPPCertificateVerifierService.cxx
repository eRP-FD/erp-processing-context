/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPCertificateVerifierService.hxx"
#include "shared/model/Health.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"

PoPPCertificateVerifierService::PoPPCertificateVerifierService(gsl::not_null<boost::asio::io_context*> context,
                                                               TslManager& tslManager, std::shared_ptr<CrlProvider> crlProvider)
    : mVerifier(std::make_shared<PoPPCertificateVerifier>(*context, tslManager, std::move(crlProvider)))
    , mVerifyTimer(std::make_unique<PeriodicTimer<PoPPCertificateVerifier>>(mVerifier))
{
}

PoPPCertificateVerifierService::PoPPCertificateVerifierService(boost::asio::io_context& context,
                                                               std::unique_ptr<UrlRequestSender> urlRequestSender,
                                                               TslManager& tslManager)
    : mVerifier(std::make_shared<PoPPCertificateVerifier>(context, std::move(urlRequestSender), tslManager))
    , mVerifyTimer(std::make_unique<PeriodicTimer<PoPPCertificateVerifier>>(mVerifier))
{
}

// GEMREQ-start A_27358#download_setup
// GEMREQ-start A_26449#timer_setup
void PoPPCertificateVerifierService::startConfigured() const
{
    start(std::chrono::seconds(Configuration::instance().getIntValue(ConfigurationKey::POPP_UPDATE_INTERVAL_SECONDS)),
          std::chrono::seconds(Configuration::instance().getIntValue(ConfigurationKey::POPP_UPDATE_MAX_AGE_SECONDS)),
          Configuration::instance().getIntValue(ConfigurationKey::POPP_CONNECTION_TIMEOUT_SECONDS),
          Configuration::instance().getIntValue(ConfigurationKey::POPP_RESPONSE_TIMEOUT_SECONDS));
}
// GEMREQ-end A_26449#timer_setup
// GEMREQ-end A_27358#download_setup

void PoPPCertificateVerifierService::start(std::chrono::steady_clock::duration verifyInterval,
                                           std::chrono::steady_clock::duration maxAgeSeconds, int connectTimeoutSeconds,
                                           int responseTimeoutSeconds,
                                           std::chrono::steady_clock::duration errorCaseInterval) const
{
    mVerifier->setup(verifyInterval, maxAgeSeconds, connectTimeoutSeconds, responseTimeoutSeconds, errorCaseInterval);

    mVerifyTimer->start(mVerifier->context(), std::chrono::seconds(0));
}

std::optional<PoPPCertificateVerifier::EntityStatement> PoPPCertificateVerifierService::getEntityStatement() const
{
    return mVerifier->getEntityStatement();
}

std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> PoPPCertificateVerifierService::getKey(const std::string& kid) const
{
    return mVerifier->getKey(kid);
}

std::list<model::PoPPCertificateHealthData> PoPPCertificateVerifierService::getHealthData() const
{
    return mVerifier->getHealthData();
}

void PoPPCertificateVerifierService::healthCheck() const
{
    const bool anyUp = std::ranges::any_of(mVerifier->getHealthData(),
                                           [&](const model::PoPPCertificateHealthData& healthData) {
                                               return ! healthData.lastOcspMaxAgeExceeded &&
                                                      healthData.lastOcspSuccess.has_value() &&
                                                      healthData.expiry > model::Timestamp::now();
                                           },
                                           {});
    Expect(anyUp, "no valid PoPP certificate is available");
}
