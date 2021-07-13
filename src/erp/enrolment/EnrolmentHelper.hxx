#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTHELPER_HXX

#include "erp/tpm/Tpm.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"
#include "erp/client/HttpsClient.hxx"


namespace hsmclient {
#include <hsmclient/ERP_Client.h>
}

class HsmIdentity;


/**
 * The TpmProxy is an abstraction of how the EnrolmentHelper accesses the TPM.
 * That can be either directly to a locally running TPM (can be port-forwareded from docker or kubernetes)
 * or via the enrolment API.
 */
class TpmProxy
{
public:
    virtual ~TpmProxy (void) = default;

    virtual Tpm::EndorsementKeyOutput getEndorsementKey (void) = 0;
    virtual Tpm::AttestationKeyOutput getAttestationKey (bool isEnrolmentActive = false) = 0;
    virtual Tpm::AuthenticateCredentialOutput authenticateCredential (Tpm::AuthenticateCredentialInput&& input) = 0;
    virtual Tpm::QuoteOutput getQuote (Tpm::QuoteInput&& input, const std::string& message) = 0;
};


/**
 * This class is not used in production. It is used in tests and for the blob-db-initialization tool.
 */
class TpmProxyApi : public TpmProxy
{
public:
    virtual Tpm::EndorsementKeyOutput getEndorsementKey (void) override;
    virtual Tpm::AttestationKeyOutput getAttestationKey (bool isEnrolmentActive = false) override;
    virtual Tpm::AuthenticateCredentialOutput authenticateCredential (Tpm::AuthenticateCredentialInput&& input) override;
    virtual Tpm::QuoteOutput getQuote (Tpm::QuoteInput&& input, const std::string& message) override;

private:
    HttpsClient createClient (void);
    Header createHeader (const HttpMethod method, std::string&& target);
};


class TpmProxyDirect : public TpmProxy
{
public:
    TpmProxyDirect (BlobCache& blobCache);
    virtual Tpm::EndorsementKeyOutput getEndorsementKey (void) override;
    virtual Tpm::AttestationKeyOutput getAttestationKey (bool isEnrolmentActive = false) override;
    virtual Tpm::AuthenticateCredentialOutput authenticateCredential (Tpm::AuthenticateCredentialInput&& input) override;
    virtual Tpm::QuoteOutput getQuote (Tpm::QuoteInput&& input, const std::string& message) override;

private:
    BlobCache& mBlobCache;
};


/**
 * Collection of lower-level functions that are used in the tee token negotiation and the enrolment process.
 * Separating the two would be nice but is not easy because of shared functionality.
 * Some functionality of Tpm and HsmClient is replicated but most of the TPM and HSM functionality used in this
 * class goes beyond what our wrappers offer.
 */
class EnrolmentHelper
{
public:
    EnrolmentHelper (
        const HsmIdentity& identity,
        const std::string& certificateFilename = "",
        const size_t _logLevel = 1);
    ~EnrolmentHelper (void);


    struct Blobs
    {
        ErpVector akName;
        ErpBlob trustedAk;
        ErpBlob trustedEk;
        ErpBlob trustedQuote;
        PcrSet pcrSet;
    };


    EnrolmentHelper::Blobs createBlobs (TpmProxy& tpm);
    ErpBlob createTeeToken (BlobCache& blobCache);

    static ErpVector getBlobName (const ErpBlob& blob);

protected:
    ErpBlob trustTpmMfr (const uint32_t generation);
    ErpBlob enrollEk (const uint32_t generation, const ErpBlob& trustedRoot, const std::vector<uint8_t>& ekCertificate);
    std::tuple<ErpBlob, // Challenge blob
        std::vector<uint8_t>, // Credential data
        std::vector<uint8_t>> // Secret
    getAkChallenge (
        const uint32_t generation,
        const ErpBlob& trustedEk,
        const std::string& akPublicKey,
        const ErpArray<TpmObjectNameLength>& akName);
    std::string getChallengeAnswer (
        TpmProxy& tpm,
        const std::vector<uint8_t>& secret,
        const std::vector<uint8_t>& encryptedCredential,
        const ErpArray<TpmObjectNameLength>& akName);
    ErpBlob enrollAk (
        const uint32_t generation,
        const ErpBlob& trustedEk,
        const ErpBlob& akChallenge,
        const std::string& akPublicKey,
        const ErpArray<TpmObjectNameLength>& akName,
        const std::string& decCredential);
    std::tuple<
        std::vector<uint8_t>,
        ErpBlob> getNonce (const uint32_t generation);
    Tpm::QuoteOutput getQuote (
        TpmProxy& tpm,
        const std::vector<uint8_t>& nonce,
        const Tpm::PcrRegisterList& pcrSet,
        const std::string& message);
    ErpBlob enrollEnclave (
        const uint32_t generation,
        const ErpArray<TpmObjectNameLength>& akName,
        const Tpm::QuoteOutput& quote,
        const ErpBlob& knownAk,
        const ErpBlob& nonce);
    ErpBlob getTeeToken (
        const ErpArray<TpmObjectNameLength>& akName,
        const Tpm::QuoteOutput& quote,
        const ErpBlob& knownAk,
        const ErpBlob& nonce,
        const ErpBlob& trustedQuote);

    const size_t logLevel;
    const std::string mCertificateFilename;
    HsmRawSession mHsmSession;
};

#endif
