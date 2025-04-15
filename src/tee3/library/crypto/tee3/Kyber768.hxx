/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_KYBER768_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_KYBER768_HXX

#include "library/crypto/SensitiveDataGuard.hxx"

namespace epa
{

class BinaryBuffer;

/**
 * The Kyber768 class hides the choice of library that is used to implement the cryptographic
 * operations that are exposed via the class' interface.
 *
 * Kyber768 is a post quantum cryptography (PQC) algorithm for a key encapsulation mechanism (KEM)
 * that in its purpose is similar to the diffie hellman key exchange (KEX), i.e. provide two parties
 * with a symmetric key without exposing the key to any listeners to the key exchange negotiation.
 *
 * PQC means that the algorithm is supposed to be equally hard to break for classic von Neumann
 * architectures and quantum computers.
 *
 * Note that the gematik specification mandates the use of a final draft version of Kyber768 that
 * has been selected by NIST. The ultimately released version (yet to come at the time of writing)
 * may differ in some details. This means that we have to be careful with updates in the
 * underlying library (currently Botan). This has already happened for the Java implementation
 * in Bouncy Castle (which will be used for at least the test client), meaning that they have
 * started to make incompatible changes on the road to implement the release version of Kyber768.
 */
class Kyber768
{
public:
    /**
     * Try to verify that the given public key is valid for Kyber768.
     * This is currently a best effort implementation as the underlying Botan function is a noop.
     * TODO: find out if a Kyber768  public key can be verified at all.
     */
    static bool isValidPublicKey(const BinaryBuffer& publicKey);

    /**
     * Create a private/public Kyber768 key pair.
     * @returns a tuple of public (BinaryBuffer) and private (SensitiveDataGuard) keys.
     */
    static std::tuple<BinaryBuffer, SensitiveDataGuard> createKeyPair();

    /**
     * The encapsulation provides a shared secret (first returned value) and an encapsulated
     * (encrypted) value that is to be passed to the other side.
     * @param publicKey The public key of the receiver to which the encapsulated message will
     *     be sent.
     * @returns a tuple of the shared secret and the encapsulated secret, in this order.
     */
    static std::tuple<SensitiveDataGuard, BinaryBuffer> encapsulate(const BinaryBuffer& publicKey);

    /**
     * Decapsulate the ciphertext that is created by `encapsulate()`.
     * @param privateKey The private key of the receiver of an encapsulated message. It must match
     *     the public key that was used by `encapsulate()`.
     * @param ciphertext A ciphertext that is returned by `encapsulate()`.
     * @returns the encapsulated shared secret.
     */
    static SensitiveDataGuard decapsulate(
        const SensitiveDataGuard& privateKey,
        const BinaryBuffer& ciphertext);
};

} // namespace epa

#endif
