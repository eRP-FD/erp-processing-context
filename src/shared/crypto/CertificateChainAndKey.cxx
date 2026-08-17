/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/CertificateChainAndKey.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/Expect.hxx"

#include <vector>

namespace
{
    std::vector<std::string> splitCertificates(const std::string& certificates)
    {
        std::vector<std::string> result;

        for (size_t index = 0;;)
        {
            size_t startIndex = certificates.find(Certificate::PEM_BEGIN_TAG, index);
            if (startIndex == std::string::npos)
                break;

            size_t endIndex = certificates.find(Certificate::PEM_END_TAG, startIndex + Certificate::PEM_BEGIN_TAG.size());
            Expect(endIndex != std::string::npos, "failed to find end marker of PEM certificate");
            endIndex += Certificate::PEM_END_TAG.size();

            std::string certificate = certificates.substr(startIndex, endIndex - startIndex) + "\n";
            result.push_back(certificate);
            index = endIndex;
        }

        return result;
    }

    std::pair<std::string, std::string> splitCertificatesIntoEndEntityAndCaCertificates(
        const std::string& certificates)
    {
        std::string caCertificates;
        std::string endEntityCertificate;

        for (std::string& certificatePem : splitCertificates(certificates))
        {
            auto certificate = X509Certificate::createFromPem(certificatePem);
            if (certificate.isCaCert())
            {
                caCertificates += certificatePem;
            }
            else
            {
                Expect(endEntityCertificate.empty(), "more than one end entity certificate in certificate list");
                endEntityCertificate = std::move(certificatePem);
            }
        }

        Expect(! endEntityCertificate.empty(), "no end entity certificate in certificate list");

        return std::make_pair(std::move(endEntityCertificate), std::move(caCertificates));
    }
}


CertificateChainAndKey::CertificateChainAndKey(
    CertificateKeyPair certificateKeyPair,
    std::string caCertificates)
  : certificateKeyPair{std::move(certificateKeyPair)},
    caCertificates{std::move(caCertificates)}
{
}


CertificateChainAndKey CertificateChainAndKey::fromPem(
    const std::string& certificates,
    const std::string& key)
{
    auto endEntityAndCaCertificates = splitCertificatesIntoEndEntityAndCaCertificates(certificates);

    return CertificateChainAndKey(
        CertificateKeyPair::fromPem(endEntityAndCaCertificates.first, key),
        std::move(endEntityAndCaCertificates.second));
}
