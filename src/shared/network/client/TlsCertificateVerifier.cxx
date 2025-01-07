/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/TlsCertificateVerifier.hxx"
#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/network/connection/RootCertificates.hxx"
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
    bool result = std::any_of(dnsNames.begin(), dnsNames.end(), [hostname](const auto& element) {
        return hostname == element;
    });
    if (! result)
    {
        TLOG(INFO) << "Rejecting TLS certificate as it doesn't have expected SAN DNS name \"" << hostname << "\".";
    }
    return result;
}

}

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
    void addCertificateCheck(const std::function<bool(const X509Certificate&)>& check)
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
    std::vector<std::function<bool(const X509Certificate&)>> mAdditionalCertificateChecks;

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
            char buffer[256];
            X509_NAME_oneline(X509_get_subject_name(errCert), buffer, sizeof buffer);
            TVLOG(2) << "error " << error << " (" << X509_verify_cert_error_string(error)
                     << " at depth " << errorDepth << " in " << buffer;
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

#if 0
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
                if (! check(certificate.value()))
                    return false;
            }
        }
        return true;
    }
};


class TlsCertificateVerifier::BundledRootCertificatesImplementation : public Implementation
{
public:
    void install(boost::asio::ssl::context& sslContext) override
    {
        Implementation::install(sslContext);

        for (std::size_t ind = 0; ind < rootCertificates.size(); ind++)
        {
            boost::system::error_code ec{};
            sslContext.add_certificate_authority(boost::asio::buffer(rootCertificates[ind]), ec);
            if (ec.failed())
            {
                LOG(ERROR) << "loading bundled CA certificates failed at index " << ind;
                throw boost::beast::system_error{ec};
            }
        }
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
            const auto hostname = std::string{SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name)};
            if (! verifySubjectAlternativeDnsName(hostname, certificate.value()))
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
            mTslManager.verifyCertificate(mTslValidationParameters.tslMode, certificate,
                                          {mTslValidationParameters.certificateType},
                                          {.mode = mTslValidationParameters.ocspCheckMode,
                                           .timeSettings = {std::nullopt, mTslValidationParameters.ocspGracePeriod},
                                           .providedOcspResponse = std::move(providedOcspResponse)});
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
                return true;
        }
        Fail("Invalid value for OcspCheckMode: " + std::to_string(static_cast<uintmax_t>(ocspCheckMode)));
    }

    static int ocspVerifyCallback(SSL* ssl, void* arg)
    {
        bool valid = false;
        try
        {
            auto* self = static_cast<TslImplementation*>(arg);
            auto certificate = X509Certificate::createFromX509Pointer(SSL_get0_peer_certificate(ssl));
            const unsigned char* ocspResponseBuffer = nullptr;
            long ocspResponseSize = SSL_get_tlsext_status_ocsp_resp(ssl, &ocspResponseBuffer);
            OcspResponsePtr providedOcspResponse = nullptr;
            if (ocspResponseSize > 0)
            {
                providedOcspResponse =
                    OcspResponsePtr(d2i_OCSP_RESPONSE(nullptr, &ocspResponseBuffer, ocspResponseSize));
            }
            valid = self->tslVerify(certificate, std::move(providedOcspResponse));
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


TlsCertificateVerifier TlsCertificateVerifier::withBundledRootCertificates()
{
    return TlsCertificateVerifier(std::make_unique<BundledRootCertificatesImplementation>());
}


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
    const std::function<bool(const X509Certificate&)>& check)
{
    mImplementation->addCertificateCheck(check);
    return *this;
}


TlsCertificateVerifier& TlsCertificateVerifier::withSubjectAlternativeDnsName(
    const std::string& name)
{
    auto check = [name](const X509Certificate& certificate) {
        return verifySubjectAlternativeDnsName(name, certificate);
    };
    return withAdditionalCertificateCheck(check);
}


TlsCertificateVerifier::TlsCertificateVerifier(std::unique_ptr<Implementation> implementation)
  : mImplementation{std::move(implementation)}
{
}
