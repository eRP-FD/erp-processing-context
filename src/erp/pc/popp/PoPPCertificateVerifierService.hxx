/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/pc/popp/PoPPCertificateVerifier.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/util/PeriodicTimer.hxx"

class TslManager;
class X509Certificate;

class IPoPPCertificateVerifierService
{
public:
    virtual ~IPoPPCertificateVerifierService() = default;

    virtual void startConfigured() const = 0;
    virtual std::optional<PoPPCertificateVerifier::EntityStatement> getEntityStatement() const = 0;
    virtual std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> getKey(const std::string& kid) const = 0;
    virtual std::list<model::PoPPCertificateHealthData> getHealthData() const = 0;
    virtual void healthCheck() const = 0;
};

class PoPPCertificateVerifierService : public IPoPPCertificateVerifierService
{
public:
    explicit PoPPCertificateVerifierService(gsl::not_null<boost::asio::io_context*> context, TslManager& tslManager,
                                            std::shared_ptr<CrlProvider>);

    PoPPCertificateVerifierService(boost::asio::io_context& context, std::unique_ptr<UrlRequestSender> urlRequestSender,
                                   TslManager& tslManager);

    void startConfigured() const override;

    void start(std::chrono::steady_clock::duration verifyInterval, std::chrono::steady_clock::duration maxAgeSeconds,
               int connectTimeoutSeconds, int responseTimeoutSeconds,
               std::chrono::steady_clock::duration errorCaseInterval = std::chrono::seconds{30}) const;

    std::optional<PoPPCertificateVerifier::EntityStatement> getEntityStatement() const override;

    std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> getKey(const std::string& kid) const override;
    std::list<model::PoPPCertificateHealthData> getHealthData() const override;
    void healthCheck() const override;

private:
    std::shared_ptr<PoPPCertificateVerifier> mVerifier;
    std::unique_ptr<PeriodicTimer<PoPPCertificateVerifier>> mVerifyTimer{};
};
