/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TPMPRODUCTION_HXX
#define ERP_PROCESSING_CONTEXT_TPMPRODUCTION_HXX

#if defined(__APPLE__) || defined(_WIN32)
#error "TpmProduction can not be compiled or run on this platform"
#endif

#include "shared/tpm/Tpm.hxx"

namespace tpmclient
{
    class Client;
}

class BlobCache;



/**
 * Wrapper around the TpmClient from the tpmclient library.
 */
class TpmProduction : public Tpm
{
public:
    static std::unique_ptr<Tpm> createInstance (BlobCache& blobCache, int32_t logLevel = 1);
    static Tpm::Factory createFactory (void);

    explicit TpmProduction (BlobCache& blobCache, const int32_t loglevel = 1);

    /**
     * Retrieves the endorsement key.
     */
    EndorsementKeyOutput getEndorsementKey (void) override;

    /**
     * Retrieves the attestation key.
     */
    AttestationKeyOutput getAttestationKey (bool isEnrolmentActive = false) override;

    /**
     * Authenticates the given credential.
     */
    AuthenticateCredentialOutput authenticateCredential (
        AuthenticateCredentialInput&& input,
        bool isEnrolmentActive = false) override;

    /**
     * Computes a quote for the given list of registers and returns it and its signature.
     */
    QuoteOutput getQuote (
        QuoteInput&& input,
        bool isEnrolmentActive = false) override;

private:
    BlobCache& mBlobCache;
    const int32_t mLogLevel;

    static std::vector<uint8_t> provideAkKeyPairBlob (
        tpmclient::Client& client,
        BlobCache& blobCache,
        const bool isEnrolmentActive = false);
};


#endif
