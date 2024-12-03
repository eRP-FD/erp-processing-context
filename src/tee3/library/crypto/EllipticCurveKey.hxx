/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_ELLIPTICCURVEKEY_HXX
#define EPA_LIBRARY_CRYPTO_ELLIPTICCURVEKEY_HXX

#include "library/wrappers/OpenSsl.hxx"

#include <cstddef>
#include <memory>

namespace epa
{

/**
 * Represents an abstraction of an elliptic curve key pair.
 *
 * An object of this class may represent:
 * - only a private key (the corresponding public key would be trivial to compute)
 * - only a public key (the corresponding private key would be impossible to compute)
 * - both the private and public keys
 *
 * Upon construction only a given elliptic curve primitive will be owned, but no keys.
 *
 * A keypair may be generated and owned by an instance by calling `generate()`.
 *
 * Alternatively, a key (or key pair) may become owned (via duplication)
 * by an instance by calling `setPublicKey()` or `setKeyPair()`.
 */
class EllipticCurveKey
{
public:
    explicit EllipticCurveKey(int nid);

    /**
     * Tells whether any public key is currently in ownership.
     */
    bool hasPublicKey() const;

    /**
     * Returns the owned public key (or throws an exception if there is not any).
     */
    const EC_POINT* getPublicKey() const;

    /**
     * Sets the currently owned public key to `publicKey`. Duplicates the argument.
     */
    void setPublicKey(const EC_POINT* publicKey);

    /**
     * Clears the currently owned public key (if there is any).
     */
    void clearPublicKey();

    /**
     * Tells whether any private key is currently in ownership.
     */
    bool hasPrivateKey() const;

    /**
     * Returns the owned private key (or throws an exception if there is not any).
     */
    const BIGNUM* getPrivateKey() const;

    /**
     * Sets the currently owned private/public key pair
     * to `privateKey`/`publicKey`. Duplicates both arguments.
     */
    void setKeyPair(const BIGNUM* privateKey, const EC_POINT* publicKey);

    /**
     * Clears the currently owned private key (if there is any).
     */
    void clearPrivateKey();

    /**
     * Generates a new private/public key pair and takes ownership of it.
     */
    void generate();

    /**
     * Retrieves the underlying elliptic curve's finite field.
     */
    const EC_GROUP* getGroup() const;

    /**
     * Computes the size of the underlying elliptic curve's finite field (in bytes).
     */
    std::size_t getFieldSize() const;

    /**
     * Returns a native handle to the OpenSSL internal structure.
     */
    EC_KEY* getNativeHandle() const;

    /**
     * Speeds up future point multiplications done using the underlying elliptic curve's
     * finite field (via `getGroup()`) by precomputing the generator multiplication table.
     */
    void computeMultiplicationTable();

    /**
     * Tells whether this object's public key
     * is the exact same point represented by `rhs`'s public key.
     *
     * Throws an exception if either object does not own a public key.
     */
    bool operator==(const EllipticCurveKey& rhs) const;

    /**
     * Tells whether this object's public key is anything
     * but NOT the exact same point represented by `rhs`'s public key.
     *
     * Throws an exception if either object does not own a public key.
     */
    bool operator!=(const EllipticCurveKey& rhs) const;

private:
    using EcKeyPtr = std::unique_ptr<EC_KEY, void (*)(struct ec_key_st*)>;

    EcKeyPtr mKey;
};
} // namespace epa

#endif
