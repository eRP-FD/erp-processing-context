#ifndef ERP_PROCESSING_CONTEXT_TPM_HXX
#define ERP_PROCESSING_CONTEXT_TPM_HXX

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class BlobCache;


/**
 * Interface for the TPM (Trusted Platform Module) and abstraction of the TPM implementation that is provided by
 * the tpmclient library. The production TPM has the limitation that there can only be one Tpm instance be talking to
 * the real or simulated TPM at any one time.
 *
 * See TpmProduction for an implementation that forwards calls to the tpmclient library.
 * As the tpmclient library currently is a stubbed implementation with calls to an external server, there is
 * no mock implementation for tests.
 *
 * Note that all methods could be made static as the functions in the tpmclient library are free functions. However, they
 * are not static to allow different implementations for production and testing.
 */
class Tpm
{
public:
    using Factory = std::function<std::unique_ptr<Tpm>(BlobCache&)>;

    struct EndorsementKeyOutput
    {
        std::string certificateBase64;
        std::string ekNameBase64;
    };

    struct AttestationKeyOutput
    {
        std::string publicKeyBase64;
        std::string akNameBase64;
    };

    struct AuthenticateCredentialInput
    {
        std::string secretBase64;
        std::string credentialBase64;
        std::string akNameBase64;
    };

    struct AuthenticateCredentialOutput
    {
        std::string plainTextCredentialBase64;
    };

    using PcrRegisterList = std::vector<size_t>;

    struct QuoteInput
    {
        std::string nonceBase64;
        PcrRegisterList pcrSet;
        std::string hashAlgorithm;
    };

    struct QuoteOutput
    {
        std::string quotedDataBase64;
        std::string quoteSignatureBase64;
    };


    Tpm (void) = default;
    virtual ~Tpm (void) = default;

    /**
     * Retrieves the endorsement key.
     */
    virtual EndorsementKeyOutput getEndorsementKey (void) = 0;

    /**
     * Retrieves the attestation key.
     */
    virtual AttestationKeyOutput getAttestationKey (
        bool isEnrolmentActive = false) = 0;

    /**
     * Authenticates the given credential.
     */
    virtual AuthenticateCredentialOutput authenticateCredential (
        AuthenticateCredentialInput&& input,
        bool isEnrolmentActive = false) = 0;

    /**
     * Computes a quote for the given list of registers and returns it and its signature.
     */
    virtual QuoteOutput getQuote (
        QuoteInput&& input,
        bool isEnrolmentActive = false) = 0;
};


std::ostream& operator<< (std::ostream& out, const Tpm::PcrRegisterList& pcrSet);


#endif
