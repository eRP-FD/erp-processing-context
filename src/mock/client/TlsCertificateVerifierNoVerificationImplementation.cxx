/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/client/TlsCertificateVerifierNoVerificationImplementation.hxx"

#include <boost/asio/ssl/context.hpp>

void TlsCertificateVerifierNoVerificationImplementation::install(boost::asio::ssl::context& sslContext)
{
    sslContext.set_verify_mode(boost::asio::ssl::verify_none);
}

TlsCertificateVerifier TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting()
{
    return TlsCertificateVerifier(std::make_unique<TlsCertificateVerifierNoVerificationImplementation>());
}
