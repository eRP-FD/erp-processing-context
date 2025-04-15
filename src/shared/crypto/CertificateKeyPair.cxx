/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/CertificateKeyPair.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"


CertificateKeyPair::CertificateKeyPair(shared_X509 certificate, shared_EVP_PKEY key)
  : mKey{std::move(key)},
    mCertificate{std::move(certificate)}
{
}


CertificateKeyPair CertificateKeyPair::fromPem(
    const std::string& pemCertificate,
    const std::string& pemKey)
{
    auto certificate = Certificate::fromPem(pemCertificate);

    auto mem = shared_BIO::make();
    const int written = BIO_write(mem, static_cast<const char*>(pemKey.data()), gsl::narrow<int>(pemKey.size()));
    OpenSslExpect(written >= 0, "reading pem private key failed");
    OpenSslExpect(static_cast<size_t>(written) == pemKey.size(), "attempt to write bytes has failed");

    auto privateKey = shared_EVP_PKEY::make();
    auto* p = PEM_read_bio_PrivateKey(mem, privateKey.getP(), nullptr, nullptr);
    Expect(p != nullptr, "failed to parse private key from PEM string");
    return CertificateKeyPair{certificate.toX509(), privateKey};
}
