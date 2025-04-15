/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEESERVERKEYSIMPLEMENTATION_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEESERVERKEYSIMPLEMENTATION_HXX

#include "library/crypto/tee3/TeeServerKeys.hxx"

namespace epa
{

/**
 * The "master" private/public key pairs, for Ecdh and Kyber, that are held by each PC server
 * instance.
 *
 * At the moment this class is designed with the assumption that the keys are created during
 * application start up (i.e. before they are used for incoming connections) and that the
 * monthly key renewal is done via application restart.
 * I.e. during regular runtime the keys are treated as const values.
 * The maximum life time of the key pairs is, per ANFEPA-2362, is 30 days.
 */
class TeeServerKeysImplementation : public TeeServerKeys
{
public:
    explicit TeeServerKeysImplementation(CryptoService& cryptoService);
    ~TeeServerKeysImplementation() override = default;

    Tee3Protocol::PrivateKeys privateKeys() const override;

    Tee3Protocol::SignedPublicKeys signedPublicKeys() const override;

    const Certificate& signingCertificate() const override;

    std::optional<Tee3Protocol::AutTeeCertificate> certificateForHash(
        const BinaryBuffer& hash,
        uint64_t version) const override;

protected:
    virtual BinaryBuffer getCaCertificate() const = 0;
    virtual std::vector<BinaryBuffer> getRcaChain() const = 0;

private:
    /**
     * The public and private key pairs for ECDH and Kyber768.
     */
    Tee3Protocol::EcdhPublicKey mEcdhPublicKey;
    SensitiveDataGuard mEcdhPrivateKey;
    BinaryBuffer mKyber768PublicKey;
    SensitiveDataGuard mKyber768PrivateKey;

    /**
     * Remember the time at which the keys have been created. In the unlikely case that
     * a pod is not restarted inside the allowed 30 days, an error is created when the
     * keys are accessed after that time.
     */
    std::chrono::system_clock::time_point mKeyCreationTime;

    /**
     * The signature of the public keys, together with data that is required for
     * verification of the signature.
     */
    BinaryBuffer mSerializedPublicKeys;
    BinaryBuffer mPublicKeySignature;
    std::unique_ptr<Certificate> mSigningCertificate;
    BinaryBuffer mSigningCertificateDer;
    BinaryBuffer mSigningCertificateHash;
    BinaryBuffer mOcspResponse;

    /**
     * A_24427: create an ECDH and a Kyber768 key pair.
     */
    void createKeyPairs();

    /**
     * A_24427: Sign both (ecdh and kyber) key pairs with the help of the HSM.
     */
    void signPublicKeys(CryptoService& cryptoService);

    /**
     * Verify that the key pairs are not older than 30 days. Otherwise an exception is thrown.
     */
    void assertKeyLifetimeValid() const;
};


/**
 * The implementation that is intended for use in production selects different certificates
 * for `ca` and `rca-chain` then the test implementation, although both use a HSM.
 * The reason is that the signing certificates are signed differently in production and for testing
 * in the HSM simulator.
 */
class TeeServerKeysProductionHsmImplementation : public TeeServerKeysImplementation
{
public:
    using TeeServerKeysImplementation::TeeServerKeysImplementation;

protected:
    BinaryBuffer getCaCertificate() const override;
    std::vector<BinaryBuffer> getRcaChain() const override;
};


class TeeServerKeysHsmSimulatorImplementation : public TeeServerKeysImplementation
{
public:
    using TeeServerKeysImplementation::TeeServerKeysImplementation;

protected:
    BinaryBuffer getCaCertificate() const override;
    std::vector<BinaryBuffer> getRcaChain() const override;
};
} // namespace epa

#endif
