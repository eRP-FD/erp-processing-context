/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSMSESSION_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSMSESSION_HXX

#include "erp/hsm/HsmClient.hxx"

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/Expect.hxx"
#include "erp/crypto/Certificate.hxx"

#include <functional>
#include <memory>
#include <optional>


class BlobCache;
class HsmRawSession;


/**
 * This class is loosely modeled after the HSM C client interface but with a stronger focus on the use cases in the
 * VAU processing context. For example, all data, that can be retrieved from the HsmContext, has been removed from the
 * methods so that they have as small a number of arguments as possible.
 */
class HsmSession
{
public:
    /**
     *
     * @param client
     * @param context
     * @param rawSession  Pointer to a hsmclient::HSMSession object or empty.
     *                    This is a shared pointer only because that allows it to have a deleter that can be created
     *                    dynamically at runtime and does not need compile time information. This is necessary because
     *                    on Mac, we don't include the C interface.
     */
    HsmSession (
        HsmClient& client,
        BlobCache& blobCache,
        std::shared_ptr<HsmRawSession>&& rawSession);

    ::Nonce getNonce(uint32_t input);

    ErpBlob generatePseudonameKey(uint32_t input);
    ErpArray<Aes256Length> unwrapPseudonameKey();

    ErpBlob wrapRawPayload(const ErpVector& rawPayload, uint32_t blobGeneration);
    ErpVector unwrapRawPayload(const ErpBlob& wrappedRawPayload);

    void setTeeToken (const ErpBlob& teeToken);

    /**
     * Derive a persistence key for a task resource.
     * On the first call for a given task resource leave `secondCallData` empty. In this case a salt value and a blob generation
     * id are returned. These are expected to be passed in on the second and all subsequent calls as `secondCallData`.
     *
     * @throws if there is any error
     */
    DeriveKeyOutput deriveTaskPersistenceKey (
        const ErpVector& derivationData,
        const std::optional<OptionalDeriveKeyData>& secondCallData = std::nullopt);

    /**
     * Derive a persistence key for a communication resource.
     * On the first call for a given task resource leave `secondCallData` empty. In this case a salt value and a blob generation
     * id are returned. These are expected to be passed in on the second and all subsequent calls as `secondCallData`.
     *
     * @throws if there is any error
     */
    DeriveKeyOutput deriveCommunicationPersistenceKey (
        const ErpVector& derivationData,
        const std::optional<OptionalDeriveKeyData>& secondCallData = std::nullopt);

    /**
     * Derive a persistence key for a audit log resource.
     * On the first call for a given task resource leave `secondCallData` empty. In this case a salt value and a blob generation
     * id are returned. These are expected to be passed in on the second and all subsequent calls as `secondCallData`.
     *
     * @throws if there is any error
     */
    DeriveKeyOutput deriveAuditLogPersistenceKey (
        const ErpVector& derivationData,
        const std::optional<OptionalDeriveKeyData>& secondCallData = std::nullopt);

    /**
     * Derive a persistence key for a charge item resource.
     * On the first call for a given charge item resource leave `secondCallData` empty. In this case a salt value and a blob generation
     * id are returned. These are expected to be passed in on the second and all subsequent calls as `secondCallData`.
     *
     * @throws if there is any error
     */
    [[nodiscard]] ::DeriveKeyOutput
    deriveChargeItemPersistenceKey(const ::ErpVector& derivationData,
                                   const ::std::optional<OptionalDeriveKeyData>& secondCallData = ::std::nullopt);

    /**
     * Return the id of the latest (newest) key derivation blob for Task (and MedicationDispense) resources.
     * This is mostly important for the decryption of MedicationDispense resources where a salt may have already been
     * generated for another MedicationDispense resource. As these solts are tied to individual blobs and a MedicationDispense
     * resource is to be encrypted with the latest blob, we have to be able to determine the id of the latest blob.
     */
    BlobId getLatestTaskPersistenceId (void) const;
    BlobId getLatestCommunicationPersistenceId (void) const;
    BlobId getLatestAuditLogPersistenceId (void) const;
    [[nodiscard]] ::BlobId getLatestChargeItemPersistenceId() const;

    /**
     * Return the requested number of random bytes.
     * The number is restrictd by hsmclient:MaxRndBytes (320).
     *
     * @throws if there is any error
     */
    ErpVector getRandomData (size_t numberOfRandomBytes);

    /**
     * Execute ECIES key derivation.
     *
     * @throws if there is any error
     */
    ErpArray<Aes128Length> vauEcies128 (const ErpVector& clientPublicKey, bool useFallback = false);

    /**
     * Get the Ecies Public Key
     *
     * @return ErpVector the public key in binary ANSI X9.62/RFC-5480 format
     */
    shared_EVP_PKEY getEciesPublicKey();

    std::tuple<shared_EVP_PKEY, ErpBlob> getVauSigPrivateKey(const shared_EVP_PKEY& cachedKey,
                                                             const ErpBlob& cachedBlob);

    [[nodiscard]] Certificate getVauSigCertificate() const;

    /**
     * Return a 256 Bit salt that is to be used in HMAC calculations of KVNRs.
     */
    ErpArray<Aes256Length> getKvnrHmacKey (void);

    /**
     * Return a 256 Bit salt that is to be used in HMAC calculations of Telematik Ids.
     */
    ErpArray<Aes256Length> getTelematikIdHmacKey (void);

    ::ParsedQuote parseQuote(const ::ErpVector& quote) const;

    ErpBlob getTeeToken(const ErpBlob& nonce, const ErpVector& quoteData, const ErpVector& quoteSignature,
                        const std::optional<ErpBlob>& knownQuote = {});

    /**
     * Make a call to the HSM to keep the connection to the HSM alive.
     * Make this call only if there was no other call after the given `threshold`.
     */
    void keepAlive (std::chrono::system_clock::time_point threshold);

    void reconnect (void);

private:
    HsmClient& mClient;
    BlobCache& mBlobCache;
    ErpBlob mTeeToken;
    std::shared_ptr<HsmRawSession> mRawSession;
    std::chrono::system_clock::time_point mLastHsmCall;

    DeriveKeyInput makeDeriveKeyInput(
        const BlobType blobType,
        const ErpVector& derivationData,
        const std::optional<OptionalDeriveKeyData>& secondCallData);

    const ErpBlob& getCachedTeeToken() const;
    void markHsmCallTime (void);
};


#endif
