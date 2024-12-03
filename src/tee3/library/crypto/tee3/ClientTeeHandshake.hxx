/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_CLIENTTEEHANDSHAKE_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_CLIENTTEEHANDSHAKE_HXX

#include "library/crypto/tee3/Tee3Context.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/crypto/tee3/TeeHandshakeBase.hxx"
#include "library/util/BinaryBuffer.hxx"

#include "shared/crypto/Certificate.hxx"

class TslManager;

namespace epa
{

/**
 * Send handshake messages "M1" and "M3" to the server and receive "M2" and "M4" respectively
 *
 * How to use:
 * 1. Create a new instance for every TEE handshake.
 * 2. Call createMessage1() and send the returned message to the TEE server.
 * 3. Receive the M2 message and call createMessage3() with it. It returns CBOR serialized M3
 *    message
 * 4. Call context() to get access to its VAU_CID member. The other values are already initialized
 *    at this point but are not yet verified and should not be used.
 * 5. Send the M3 message to the server. Use the VAU-CID as URL path.
 * 6. Receive the M4 message and call processMessage4() with it.
 * 7. You are free to use the context, obtained in step 4, to use for encrypting messages
 *    to an ePA document service.
 */
class ClientTeeHandshake : protected TeeHandshakeBase
{
public:
    /**
     * An abbreviation for a functor that provides the certificate that is identified by `hash`
     * and `version`.  This is typically the `cert` value from the structure returned by the
     * TEE server's /CertData.<hash>-<version> endpoint.  The functor would also be responsible
     * to verify the returned certificate.
     */
    using CertificateProvider =
        std::function<Certificate(const BinaryBuffer& hash, uint64_t version)>;

    /**
     * The `certificateProvider` is called only when the certificate that signed the server's
     * public keys is not present in the client.
     */
    explicit ClientTeeHandshake(CertificateProvider certificateProvider, TslManager& tslManager, bool isPU);

    /**
     * Create an M1 message with a random public key.
     * The returned message is to be sent as body to the POST /VAU endpoint of the record
     * system (per A_24428) with mime type "application/cbor".
     */
    BinaryBuffer createMessage1();

    /**
     * The VAU-CID is transported via response header alongside message 2. That means that it
     * has to be provided (so that it can be integrated in the object that is returned by context())
     * explicitly with this method.
     */
    void setVauCid(const Tee3Protocol::VauCid& vauCid);

    /**
     * Process the server's M2 message and create an M3 message.
     * Send the M3 message as body to the POST /${VAU-CID} endpoint of the record
     * system (per A_24623). Note, that VAU-CID is accessible via `context().VAU_CID`.
     * Returns the CBOR serialized M3 message.
     */
    BinaryBuffer createMessage3(const BinaryBuffer& serializedMessage2);

    /**
     * Verify the server's M4 message.
     */
    void processMessage4(const BinaryBuffer& serializedMessage4);

    /**
     * Return the Tee3Context that is set up during the TEE handshake.
     * Note that before `createMessage3()` returns, it is only partially initialized.
     * And only after `processMessage4()` returns, the handshake is regarded as complete and the
     * encryption keys can be used to encrypt non-handshake requests.
     */
    std::shared_ptr<Tee3Context> context() const;

    /**
     * Return the VAU-CID value.
     */
    const Tee3Protocol::VauCid& vauCid() const;

    const Tee3Protocol::SymmetricKeys& k2KeyConfirmation() const;

private:
    enum class State
    {
        NotStarted,
        Message1Created,
        Message3Created,
        Finished,
        Error
    };
    State mState;
    BinaryBuffer mTransscript;
    Tee3Protocol::Keypairs mEphemeralKeys;
    Tee3Protocol::SymmetricKeys mK2KeyConfirmation;
    CertificateProvider mCertificateProvider;
    Tee3Protocol::SymmetricKeys mK2ApplicationData;
    std::optional<KeyId> mKeyId;
    Tee3Protocol::VauCid mVauCid;
    std::shared_ptr<Tee3Context> mContext;
    TslManager& mTslManager;
    bool mIsPU;

    void verifyPublicKeys(const Tee3Protocol::SignedPublicKeys& publicKeys);
};
} // namespace epa

#endif
