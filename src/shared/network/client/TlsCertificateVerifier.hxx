/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_SHARED_NETWORK_CLIENT_TLSCERTIFICATEVERIFIER_HXX
#define ERP_SHARED_NETWORK_CLIENT_TLSCERTIFICATEVERIFIER_HXX

#include "shared/tsl/OcspCheckDescriptor.hxx"
#include "shared/tsl/TslMode.hxx"
#include "shared/util/CertificateType.hxx"

#include <boost/asio/ssl/verify_context.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>


namespace boost::asio::ssl
{
    //NOLINTNEXTLINE(readability-identifier-naming)
    class context;
}


class CrlProvider;
class X509Certificate;
class TslManager;


class TlsCertificateVerifier
{
public:
    class Implementation;
    explicit TlsCertificateVerifier(std::unique_ptr<Implementation> implementation);

    /**
     * All cryptography must comply with gematik standards, but the bundled root CA list will not
     * be used. Instead, specific root certificates are to be used.
     */
    static TlsCertificateVerifier withCustomRootCertificates(std::string customRootCertificates);

    struct TslValidationParameters {
        TslMode tslMode = TslMode::TSL;
        // passing std::nullopt disables certificate type restrictions
        std::optional<CertificateType> certificateType;
        OcspCheckDescriptor::OcspCheckMode ocspCheckMode = OcspCheckDescriptor::PROVIDED_OR_CACHE;
        std::chrono::system_clock::duration ocspGracePeriod;
        // performs a hostname validation based on the host set in SNI
        // with the Subject Alternative Addresses from the certificate
        bool withSubjectAlternativeAddressValidation;
        bool allowTslUpdate = true;
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
        const std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>& check);

    /**
     * Instruct the verifier to additionally require a verified end-entity certificate to contain
     * the given SubjectAlternativeName DNS name.
     */
    TlsCertificateVerifier& withSubjectAlternativeDnsName(const std::string& name);

    /**
     * Use the passed CRL source for crl validation
     */
    enum class CrlMode
    {
        SOFT_FAIL,
        HARD_FAIL,
    };
    TlsCertificateVerifier& withCrl(CrlProvider& crl, CrlMode mode);

    /**
     * Base class for certificate verification implementations. Two methods can be overridden to
     * implement a strategy: install() is called when the context for a TLS connection is prepared, to
     * install a verification callback and set a verification mode. verifyCertificate() is called to
     * verify individual peer certificates.
     *
     * An instance of this class is only used for a single TLS connection.
     */
    class Implementation
    {
    public:
        virtual ~Implementation() = default;

        /**
         * Called when preparing the context for a TLS connection. It should set the verification mode
         * and verification callback on the OpenSSL SSL context. The default implementation sets the
         * verification mode such that the remote peer is required to send a certificate and it installs
         * a verification callback, so that verifyCertificate() is called for each certificate provided
         * by the peer.
         *
         * @param sslContext The SSL context on which to set the verification mode and callback.
         */
        virtual void install(boost::asio::ssl::context& sslContext);

        /**
         * Add a function that performs an additional check of the certificate. It will be invoked for
         * an end-entity certificate to be verified that already passed the regular verification
         * performed by the verifier. If any additional certificate check returns false the certificate
         * will be rejected.
         *
         * @param check The function to be invoked to perform the check. Returns true to accept the
         *        certificate, false to reject it.
         */
        void
        addCertificateCheck(const std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>& check);

    protected:
        /**
         * During the TLS handshake, this method is invoked for each certificate of the certificate
         * chain provided by the remote peer. The method is implemented by derived classes according to
         * their certificate verification strategy.
         *
         * When the method is called, OpenSSL has already done some verification of the certificate
         * depending on parameters set on the SSL context. The boolean result of this verification is
         * supplied as an argument. It can be used or ignored as required by the implementation, but it
         * doesn't have any effect beyond that -- only the return value of this method decides whether
         * the certificate is accepted. The default implementation simply returns the given
         * pre-verification result.
         *
         * @param context The verification context. A native OpenSSL handle can be retrieved from it.
         * @param certificate The certificate to be verified. May be the peer's end entity certificate
         *        or any certificate of the chain it provided.
         * @param preVerified The result of the verification of the certificate performed by OpenSSL.
         *        May be ignored or used as needed by the implementation.
         * @return true, if the certificate (and thus the TLS connection) shall be accepted, false if it
         *         shall be rejected.
         */
        virtual bool verifyCertificate(
            boost::asio::ssl::verify_context& /*context*/,
            const std::optional<X509Certificate>& /*certificate*/,
            bool preVerified);

        void installVerifyCallback(boost::asio::ssl::context& sslContext);

        static bool isEndEntityCertificate(
            boost::asio::ssl::verify_context& context,
            const X509Certificate& certificate);

    private:
        std::vector<std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>> mAdditionalCertificateChecks;

        static std::optional<X509Certificate> getCertificateFromSslContext(
            boost::asio::ssl::verify_context& context);

        static void logVerifyCertificateResult(
            bool preVerified,
            bool result,
            boost::asio::ssl::verify_context& context,
            const std::optional<X509Certificate>& certificate);

        bool verifyCertificateCallback(
            bool preVerified,
            boost::asio::ssl::verify_context& context);

        bool performAdditionalChecks(
            boost::asio::ssl::verify_context& context,
            const std::optional<X509Certificate>& certificate);
    };

private:
    class BundledRootCertificatesImplementation;
    class CustomRootCertificatesImplementation;
    class TslImplementation;
    class NoVerificationImplementation;

    std::shared_ptr<Implementation> mImplementation;
};


#endif
