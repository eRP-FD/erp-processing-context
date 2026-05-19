/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/network/client/TlsCertificateVerifier.hxx"

#if WITH_HSM_MOCK != 1
#error TlsCertificateVerifierNoVerificationImplementation.hxx included but WITH_HSM_MOCK not enabled
#endif

/**
 * Certificate verification implementation that accepts any certificate, not verifying anything.
 * Used for testing purposes only, but located in production code so it can be used in certain
 * integration environments.
 */
class TlsCertificateVerifierNoVerificationImplementation : public TlsCertificateVerifier::Implementation
{
public:
    void install(boost::asio::ssl::context& sslContext) override;
    static TlsCertificateVerifier withVerificationDisabledForTesting();
};
