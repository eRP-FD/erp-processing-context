/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/TlsCertificateVerifier.hxx"
#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/network/client/CrlProvider.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/error.hpp>
#include <optional>

namespace
{

bool verifySubjectAlternativeDnsName(const std::string& hostname, const X509Certificate& certificate)
{
    auto dnsNames = certificate.getSubjectAlternativeNameDnsNames();
    bool result = std::ranges::any_of(dnsNames, [hostname](const auto& element) {
        return hostname == element;
    });
    if (! result)
    {
        TLOG(INFO) << "Rejecting TLS certificate as it doesn't have expected SAN DNS name \"" << hostname << "\".";
    }
    return result;
}


// GEMREQ-start A_27857#crl
void validateCrl(shared_X509_CRL& crl, X509Certificate& issuer, const X509Certificate& cert)
{
    Expect(crl != nullptr, "Empty CRL");
    // following rfc5280, 6.3.33, skipping deltas and reason marks
    auto crlIssuer = x509NametoString(X509_CRL_get_issuer(crl));
    Expect(cert.getIssuer() == crlIssuer, "CRL issuer does not match certificate issuer");
    Expect(issuer.checkKeyUsage({KeyUsage::cRLSign}), "CRL issuer missing key usage cRLSign");

    auto* issuerPKey = issuer.getPublicKey();
    auto ret = X509_CRL_verify(crl, issuerPKey);
    Expect(ret == 1, "CRL signature validation failed");
}
// GEMREQ-end A_27857#crl

}//namespace

/**
 * Base class for certificate verification implementations. Two methods can be overridden to
 * implement a strategy: install() is called when the context for a TLS connection is prepared, to
 * install a verification callback and set a verification mode. verifyCertificate() is called to
 * verify individual peer certificates.
 *
 * An instance of this class is only used for a single TLS connection.
 */
class TlsCertificateVerifier::Implementation
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
    virtual void install(boost::asio::ssl::context& sslContext)
    {
        sslContext.set_verify_mode(
            boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);

        installVerifyCallback(sslContext);
    }

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
    addCertificateCheck(const std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>& check)
    {
        mAdditionalCertificateChecks.push_back(check);
    }

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
        bool preVerified)
    {
        return preVerified;
    }

    void installVerifyCallback(boost::asio::ssl::context& sslContext)
    {
        sslContext.set_verify_callback(
            [this](bool preVerified, boost::asio::ssl::verify_context& context) {
                return verifyCertificateCallback(preVerified, context);
            });
    }

    static bool isEndEntityCertificate(
        boost::asio::ssl::verify_context& context,
        const X509Certificate& certificate)
    {
        STACK_OF(X509)* certificateChain = X509_STORE_CTX_get0_chain(context.native_handle());
        if (sk_X509_num(certificateChain) <= 0)
        {
            LOG(ERROR) << "The certificate chain is empty.";
            throw std::runtime_error("The certificate chain is empty.");
        }
        return certificate.getX509ConstPtr() == sk_X509_value(certificateChain, 0);
    }

private:
    std::vector<std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>> mAdditionalCertificateChecks;

    static std::optional<X509Certificate> getCertificateFromSslContext(
        boost::asio::ssl::verify_context& context)
    {
        try
        {
            return X509Certificate::createFromX509Pointer(
                X509_STORE_CTX_get_current_cert(context.native_handle()));
        }
        catch (const std::exception& exception)
        {
            LOG(ERROR) << "failed to get server certificate from SSL context: " << exception.what();
        }

        return std::nullopt;
    }

    static void logVerifyCertificateResult(
        bool preVerified,
        bool result,
        boost::asio::ssl::verify_context& context,
        const std::optional<X509Certificate>& certificate)
    {
        std::string serialNumber = "?";
        std::string subject = "?";

        if (certificate.has_value())
        {
            try
            {
                serialNumber = certificate->getSerialNumber();
            }
            catch (const std::exception& exception)
            {
                LOG(ERROR) << "Failed to get serial number from certificate: " << exception.what();
            }

            try
            {
                subject = certificate->getSubject();
            }
            catch (const std::exception& exception)
            {
                LOG(ERROR) << "Failed to get subject from certificate: " << exception.what();
            }
        }

        static const auto boolToSuccessString = [](bool value) {
            return value ? "success" : "failed";
        };

        TVLOG(1) << "TLS certificate check, subject = " << subject << "\n"
            << "  Result: " << boolToSuccessString(result)
            << " (pre-verified: " << boolToSuccessString(preVerified) << ")\n"
            << "  Serial no: " << serialNumber;

        // In case of TLS certificate validation issues log more details on log level INFO2.
        if (! result)
        {
            const int error = X509_STORE_CTX_get_error(context.native_handle());
            const int errorDepth = X509_STORE_CTX_get_error_depth(context.native_handle());
            X509* errCert = X509_STORE_CTX_get_current_cert(context.native_handle());
            TVLOG(2) << "error " << error << " (" << X509_verify_cert_error_string(error)
                     << " at depth " << errorDepth << " in " << x509NametoString(X509_get_subject_name(errCert));
        }
    }

    bool verifyCertificateCallback(
        const bool preVerified,
        boost::asio::ssl::verify_context& context)
    {
        // get the certificate
        std::optional<X509Certificate> certificate = getCertificateFromSslContext(context);

        // verify the certificate
        bool result = false;
        try
        {
            result = verifyCertificate(context, certificate, preVerified)
                     && performAdditionalChecks(context, certificate);
        }
        catch (const std::exception& exception)
        {
            LOG(ERROR) << "failed to verify server certificate: " << exception.what();
        }

        if (! result)
        {
            LOG(WARNING) << "validation of server certficate failed: "
                         << (certificate.has_value() ? certificate->toBase64() : "");
        }

#if 0 // NOLINT(readability-avoid-unconditional-preprocessor-if)
        // this callback is registered to better debug certificate verification problems,
        // it is not necessary for production and should be disabled
        logVerifyCertificateResult(preVerified, result, context, certificate);
#endif

        return result;
    }

    bool performAdditionalChecks(
        boost::asio::ssl::verify_context& context,
        const std::optional<X509Certificate>& certificate)
    {
        if (certificate.has_value() && isEndEntityCertificate(context, *certificate))
        {
            for (const auto& check : mAdditionalCertificateChecks)
            {
                if (! check(certificate.value(), context))
                {
                    return false;
                }
            }
        }
        return true;
    }
};


class TlsCertificateVerifier::CustomRootCertificatesImplementation : public Implementation
{
public:
    explicit CustomRootCertificatesImplementation(std::string customRootCertificates)
      : mRootCertificates{std::move(customRootCertificates)}
    {
    }

    void install(boost::asio::ssl::context& sslContext) override
    {
        Implementation::install(sslContext);

        boost::system::error_code ec{};
        sslContext.add_certificate_authority(boost::asio::buffer(mRootCertificates), ec);
        if (ec.failed())
        {
            LOG(ERROR) << "loading custom CA certificates failed";
            throw boost::beast::system_error{ec};
        }
    }

private:
    std::string mRootCertificates;
};


class TlsCertificateVerifier::TslImplementation : public Implementation
{
public:
    explicit TslImplementation(TslManager& tslManager, TslValidationParameters tslValidationParameters)
      : mTslManager{tslManager}
      , mTslValidationParameters{tslValidationParameters}
    {
    }

    void install(boost::asio::ssl::context& sslContext) override
    {
        Implementation::install(sslContext);
        if (useProvidedOcspResponse(mTslValidationParameters.ocspCheckMode))
        {
            // setting TLSEXT_STATUSTYPE_ocsp, enabled the ocspVerifyCallback to
            // be executed after verifyCallback(), regardless whether we have
            // received an ocsp response or not
            OpenSslExpect(SSL_CTX_set_tlsext_status_type(sslContext.native_handle(), TLSEXT_STATUSTYPE_ocsp) == 1,
                          "SSL_CTX_set_tlsext_status_type failed");
            OpenSslExpect(
                SSL_CTX_set_tlsext_status_cb(sslContext.native_handle(), &TslImplementation::ocspVerifyCallback) == 1,
                "SSL_CTX_set_tlsext_status_cb failed");
            OpenSslExpect(SSL_CTX_set_tlsext_status_arg(sslContext.native_handle(), this) == 1,
                          "SSL_CTX_set_tlsext_status_arg failed");
        }
    }

protected:
    bool verifyCertificate(
        boost::asio::ssl::verify_context& context,
        const std::optional<X509Certificate>& certificate,
        bool /*preVerified*/) override
    {
        if (! certificate.has_value())
            return false;
        // We need to ignore the CA certificates and only check the end entity certificate against
        // our trust store.
        if (! isEndEntityCertificate(context, *certificate))
        {
            TLOG(INFO) << "Skipping CA certificate.";
            return true;
        }

        if (mTslValidationParameters.withSubjectAlternativeAddressValidation)
        {
            SSL* ssl = static_cast<SSL*>(
                ::X509_STORE_CTX_get_ex_data(context.native_handle(), ::SSL_get_ex_data_X509_STORE_CTX_idx()));
            OpenSslExpect(ssl != nullptr, "Unable to obtain ssl handle");
            // use the params set by TlsSession
            auto flags = X509_VERIFY_PARAM_get_hostflags(SSL_get0_param(ssl));
            const auto hostname = std::string{SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name)};
            const auto* x509Certificate = certificate->getX509ConstPtr();
            if (X509_check_host(const_cast<X509*>(x509Certificate),// NOLINT(cppcoreguidelines-pro-type-const-cast)
                                hostname.c_str(), hostname.length(), flags, nullptr) != 1)
            {
                return false;
            }
        }

        if (useProvidedOcspResponse(mTslValidationParameters.ocspCheckMode))
        {
            // the actual validation happens in ocspVerifyCallback
            return true;
        }

        return tslVerify(certificate.value(), {});
    }

    bool tslVerify(const X509Certificate& certificate, OcspResponsePtr providedOcspResponse)
    {
        try
        {
            std::unordered_set<CertificateType> typeRestrictions;
            if (mTslValidationParameters.certificateType.has_value()) {
                typeRestrictions.insert(mTslValidationParameters.certificateType.value());
            }
            mTslManager.verifyCertificate(mTslValidationParameters.tslMode, certificate,
                                          typeRestrictions,
                                          {.mode = mTslValidationParameters.ocspCheckMode,
                                           .timeSettings = {std::nullopt, mTslValidationParameters.ocspGracePeriod},
                                           .providedOcspResponse = std::move(providedOcspResponse)},
                                           mTslValidationParameters.allowTslUpdate);
            return true;
        }
        catch (const TslError&)
        {
            return false;
        }
    }

    static bool useProvidedOcspResponse(OcspCheckDescriptor::OcspCheckMode ocspCheckMode)
    {
        switch (ocspCheckMode)
        {
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_STRICT:
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_ALLOW_CACHE:
            case OcspCheckDescriptor::CACHED_ONLY:
                return false;
            case OcspCheckDescriptor::PROVIDED_ONLY:
            case OcspCheckDescriptor::PROVIDED_OR_CACHE:
            case OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED:
            case OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_INVALID:
                return true;
        }
        Fail("Invalid value for OcspCheckMode: " + std::to_string(static_cast<uintmax_t>(ocspCheckMode)));
    }

    // GEMREQ-start A_24913#ocsp-callback
    static int ocspVerifyCallback(SSL* ssl, void* arg)
    {
        bool valid = false;
        try
        {
            auto* self = static_cast<TslImplementation*>(arg);
            auto certificate = X509Certificate::createFromX509Pointer(SSL_get0_peer_certificate(ssl));
            const unsigned char* ocspResponseBuffer = nullptr;
            long ocspResponseSize = SSL_get_tlsext_status_ocsp_resp(ssl, static_cast<void*>(&ocspResponseBuffer));
            OcspResponsePtr providedOcspResponse = nullptr;
            if (ocspResponseSize > 0)
            {
                providedOcspResponse =
                    OcspResponsePtr(d2i_OCSP_RESPONSE(nullptr, &ocspResponseBuffer, ocspResponseSize));
            }
            valid = self->tslVerify(certificate, std::move(providedOcspResponse));
            // GEMREQ-end A_24913#ocsp-callback
        }
        catch (const std::exception& e)
        {
            LOG(ERROR) << "error in ocsp staping response handling: " << e.what();
        }
        return valid ? 1 : 0;
    }

private:
    TslManager& mTslManager;
    TslValidationParameters mTslValidationParameters;
};


/**
 * Certificate verification implementation that accepts any certificate, not verifying anything.
 * Used for testing purposes only, but located in production code so it can be used in certain
 * integration environments.
 */
class TlsCertificateVerifier::NoVerificationImplementation : public Implementation
{
public:
    void install(boost::asio::ssl::context& sslContext) override
    {
        sslContext.set_verify_mode(boost::asio::ssl::verify_none);
    }
};


TlsCertificateVerifier TlsCertificateVerifier::withCustomRootCertificates(
    std::string customRootCertificates)
{
    return TlsCertificateVerifier(
        std::make_unique<CustomRootCertificatesImplementation>(std::move(customRootCertificates)));
}


TlsCertificateVerifier TlsCertificateVerifier::withTslVerification(TslManager& tslManager, TslValidationParameters tslValidationParameters)
{
    return TlsCertificateVerifier(std::make_unique<TslImplementation>(tslManager, tslValidationParameters));
}


TlsCertificateVerifier TlsCertificateVerifier::withVerificationDisabledForTesting()
{
    return TlsCertificateVerifier(std::make_unique<NoVerificationImplementation>());
}


void TlsCertificateVerifier::install(boost::asio::ssl::context& sslContext)
{
    mImplementation->install(sslContext);
}


TlsCertificateVerifier& TlsCertificateVerifier::withAdditionalCertificateCheck(
    const std::function<bool(const X509Certificate&, boost::asio::ssl::verify_context&)>& check)
{
    mImplementation->addCertificateCheck(check);
    return *this;
}


TlsCertificateVerifier& TlsCertificateVerifier::withSubjectAlternativeDnsName(
    const std::string& name)
{
    auto check = [name](const X509Certificate& certificate, boost::asio::ssl::verify_context&) {
        return verifySubjectAlternativeDnsName(name, certificate);
    };
    return withAdditionalCertificateCheck(check);
}


TlsCertificateVerifier& TlsCertificateVerifier::withCrl(CrlProvider& crl)
{
    auto check = [&crl](const X509Certificate& /*certificate*/, boost::asio::ssl::verify_context& context) -> bool {
        STACK_OF(X509)* chain = X509_STORE_CTX_get0_chain(context.native_handle());
        const int chainLength = sk_X509_num(chain);
        for (int i = 0; i < chainLength - 1; ++i)
        {
            auto cert = X509Certificate::createFromX509Pointer(sk_X509_value(chain, i));
            auto issuer = X509Certificate::createFromX509Pointer(sk_X509_value(chain, i + 1));

            if (cert.getHttpCrlDistributionPoints().empty())
            {
                continue;
            }
            auto crlPtr = crl.getCrl(cert);
            try
            {
                validateCrl(crlPtr, issuer, cert);
            }
            catch (const std::exception& e)
            {
                LOG(WARNING) << "CRL validation failed for '" << cert.getSubject() << "', reason: " << e.what();
                return false;
            }

            X509_REVOKED* revoked = nullptr;
            // GEMREQ-start A_27857#validate
            // find the serial in the crl
            Expect(! X509_CRL_get0_by_cert(crlPtr, &revoked, cert.getX509Ptr()),
                   "Certificate with subject '" + cert.getSubject() + "' is revoked by CRL");
            // GEMREQ-end A_27857#validate
        }
        return true;

    };
    return withAdditionalCertificateCheck(check);
}


TlsCertificateVerifier::TlsCertificateVerifier(std::unique_ptr<Implementation> implementation)
  : mImplementation{std::move(implementation)}
{
}
