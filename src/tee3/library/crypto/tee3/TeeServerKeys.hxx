/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEESERVERKEYS_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEESERVERKEYS_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <mutex>
#include <tuple>

#include "shared/crypto/Certificate.hxx"

namespace epa
{

class CryptoService;

/**
 * Each TEE instance has one private/public key pair for ECDH and one for Kyber768. They
 * are the basis of all TEE channels that terminate in the TEE instance.
 */
class TeeServerKeys
{
public:
    virtual ~TeeServerKeys() = default;

    virtual Tee3Protocol::PrivateKeys privateKeys() const = 0;
    virtual Tee3Protocol::SignedPublicKeys signedPublicKeys() const = 0;

    /**
     * This method is intended to answer the /Cert.<hash>-<version> endpoint (A_24957).
     * Returns an empty optional when the certificate can not be found for the given
     * `hash` and `version`.
     */
    virtual std::optional<Tee3Protocol::AutTeeCertificate> certificateForHash(
        const BinaryBuffer& hash,
        const uint64_t version) const = 0;

    /// For testing
    virtual const Certificate& signingCertificate() const = 0;
};

} // namespace epa
#endif
