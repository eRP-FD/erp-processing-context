/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "PoPPCertificateVerifier.hxx"
#include "PoPPToken.hxx"


class IPoPPCertificateVerifierService;

class PoPPVerifier
{
public:
    explicit PoPPVerifier();

    static void verifyTokenSignature(const IPoPPCertificateVerifierService& poppService, const PoPPToken& poppToken);
    void verifyTokenClaims(const PoPPToken& poppToken, const PoPPCertificateVerifier::EntityStatement& entityStatement,
                           const JWT& accessToken) const;

private:
    std::chrono::seconds mIatMaxAgeSeconds;
};
