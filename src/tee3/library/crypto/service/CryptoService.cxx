/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/service/CryptoService.hxx"
#include "library/crypto/EllipticCurve.hxx" // for toString(const X509_NAME*)
#include "library/log/Logging.hxx"

namespace epa
{

// GEMREQ-start A_21889
bool CryptoService::isProductionEnvironment() const
{
    // According to GS-A_4589, the common name of a certificate in the test environment must end in
    // "TEST-ONLY". Because the common name of a certificate is printed last, we can use a string
    // suffix check to look for the "TEST-ONLY" string.
    // (False positives are fine! It just means we will be secure in an environment where we don't
    // have to be.)
    const Certificate cert = Certificate::fromPem(getSignerCertificateInPemFormat());
    if (const X509_NAME* issuer = cert.getIssuerName())
        return ! toString(issuer).ends_with(" TEST-ONLY");
    else
        return true; // secure by default
}
// GEMREQ-end A_21889


/**
 * Note that the results of the operations are not checked, only that the operations
 * complete without exception.
 */
void CryptoService::quickCheck(const std::optional<std::string>& providerKeyName) const
{
    const size_t count = 100;
    TLOG(INFO2) << "getting " << count << " random bytes from HSM";
    randomBytes(count);

    const std::string data = "this is a HSM test";
    TLOG(INFO2) << "signing data via HSM";
    std::unique_ptr<Signature> signature = sign(data, CertificateType::C_FD_SIG);

    const std::string loggingData = "this is a logging signature test";
    TLOG(INFO2) << "signing log data";
    std::unique_ptr<Signature> loggingSignature = sign(loggingData, CertificateType::C_FD_AUT);

    TLOG(INFO2) << "getting signer certificate from HSM";
    Certificate::fromPem(getSignerCertificateInPemFormat());

    if (providerKeyName.has_value())
    {
        TLOG(INFO2) << "deriving key via HSM";
        deriveKey(data, providerKeyName.value());
    }
    else
    {
        TLOG(INFO2) << "skipping deriving key via HSM";
    }

    TLOG(INFO2) << "all HSM tests finished successfully";
}

} // namespace epa
