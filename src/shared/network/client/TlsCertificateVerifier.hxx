/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_SHARED_NETWORK_CLIENT_TLSCERTIFICATEVERIFIER_HXX
#define ERP_SHARED_NETWORK_CLIENT_TLSCERTIFICATEVERIFIER_HXX

#include "shared/tsl/OcspCheckDescriptor.hxx"
#include "shared/tsl/TslMode.hxx"
#include "shared/util/CertificateType.hxx"

#include <chrono>
#include <functional>
#include <memory>
#include <string>


namespace boost::asio::ssl
{
    //NOLINTNEXTLINE(readability-identifier-naming)
    class context;
}


class X509Certificate;
class TslManager;


class TlsCertificateVerifier
{
public:
    /**
     * All cryptography must comply with gematik standards, and the bundled root CA list is used.
     * (Derived from the Mozilla root CA list; see RootCertificates.hxx.)
     */
    static TlsCertificateVerifier withBundledRootCertificates();

    /**
     * All cryptography must comply with gematik standards, but the bundled root CA list will not
     * be used. Instead, specific root certificates are to be used.
     */
    static TlsCertificateVerifier withCustomRootCertificates(std::string customRootCertificates);

    struct TslValidationParameters {
        TslMode tslMode = TslMode::TSL;
        CertificateType certificateType;
        OcspCheckDescriptor::OcspCheckMode ocspCheckMode = OcspCheckDescriptor::PROVIDED_OR_CACHE;
        std::chrono::system_clock::duration ocspGracePeriod;
        // performs a hostname validation based on the host set in SNI
        // with the Subject Alternative Addresses from the certificate
        bool withSubjectAlternativeAddressValidation;
    };
    /**
     * All cryptography must comply with gematik standards. The end-entity certificate is verified
     * against the TSL (i.e. the CA certificates it contains) and it must also comply with the given
     * certificate type.
     *
     * @param certificateType The type of certificate expected. This implies e.g. a certain
     *        certificate policy.
     */
    static TlsCertificateVerifier withTslVerification(TslManager& tslManager, TslValidationParameters tslValidationParameters);

    /**
     * All cryptography must comply with gematik standards, but the server certificate is not
     * validated at all.
     * Warning: This must only be used for tests!
     */
    static TlsCertificateVerifier withVerificationDisabledForTesting();

    TlsCertificateVerifier() = delete;

    void install(boost::asio::ssl::context& sslContext);

    /**
     * Add a function that performs an additional check of the certificate. It will be invoked for
     * an end-entity certificate to be verified that already passed the regular verification
     * performed by the verifier. If any additional certificate check returns false the certificate
     * will be rejected.
     *
     * @param check The function to be invoked to perform the check. Returns true to accept the
     *        certificate, false to reject it.
     */
    TlsCertificateVerifier& withAdditionalCertificateCheck(
        const std::function<bool(const X509Certificate&)>& check);

    /**
     * Instruct the verifier to additionally require a verified end-entity certificate to contain
     * the given SubjectAlternativeName DNS name.
     */
    TlsCertificateVerifier& withSubjectAlternativeDnsName(const std::string& name);

private:
    class Implementation;
    class BundledRootCertificatesImplementation;
    class CustomRootCertificatesImplementation;
    class TslImplementation;
    class NoVerificationImplementation;

    explicit TlsCertificateVerifier(std::unique_ptr<Implementation> implementation);

    std::shared_ptr<Implementation> mImplementation;
};


#endif
