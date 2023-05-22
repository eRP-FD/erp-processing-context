/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_ERP_PROCESSING_CONTEXT_CRYPTO_MOCK_OCSP_MOCKOCSP_HXX
#define TEST_ERP_PROCESSING_CONTEXT_CRYPTO_MOCK_OCSP_MOCKOCSP_HXX

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/crypto/Certificate.hxx"

#include <string>
#include <boost/core/noncopyable.hpp>
#include <chrono>
#include <vector>

#if WITH_HSM_MOCK  != 1
#error MockOcsp.hxx included but WITH_HSM_MOCK not enabled
#endif

/**
 * This is a super-simple mock for OCSP responses.
 *
 * It exists only to create an OCSP response string for tests that around TEE handshake.
 *
 * See EPA-16774 for moving the MockCryptoService to the test/ tree.
 */
class MockOcsp
    : private boost::noncopyable
{
public:
    enum class CertificateOcspTestMode
    {
        SUCCESS,
        REVOKED,
        CERTHASH_MISMATCH,
        CERTHASH_MISSING,
        WRONG_CERTID,
        WRONG_PRODUCED_AT,
        WRONG_THIS_UPDATE
    };

    class CertificatePair
    {
    public:
        Certificate certificate;
        Certificate issuer;
        CertificateOcspTestMode testMode;
    };

    /**
     * Create a test OCSP response based on ocsp request, known certificate/CA pairs, ocsp certificate and key.
     */
    static MockOcsp create (
        const std::string& ocspRequest,
        const std::vector<CertificatePair>& ocspResponderKnownCertificateCaPairs,
        Certificate& certificate,
        shared_EVP_PKEY& privateKey);

    /**
     * Convert an OCSP in PEM string representation into an OCSP_RESPONSE pointer (wrapped by MockOcsp).
     * This should work even for more complex OCSP responses.
     */
    static MockOcsp fromPemString(const std::string& pem);

    /**
     * Convert the OCSP response into a PEM string.
     * This should work even for more complex OCSP responses.
     */
    std::string toPemString (void);

    /**
     * Convert the OCSP response into a DER string.
     * This should work even for more complex OCSP responses.
     */
    std::string toDer (void);

    /**
     * Verify that the OCSP response is still valid, i.e. has not been issued more than `maximumAge` ago.
     */
    bool verify (const std::chrono::duration<int> maximumAge) const;

    /**
     * Verify that the given certificate still valid, given the OCSP response.
     * Other information of the certificate is not verified.
     */
    bool verify (const Certificate& certificate) const;

private:
    shared_OCSP_RESPONSE mOcspResponse;

    explicit MockOcsp (shared_OCSP_RESPONSE ocspResponse);
};


#endif
