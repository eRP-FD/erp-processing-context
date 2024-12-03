/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_ELLIPTICCURVEKEY_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_ELLIPTICCURVEKEY_HXX

#include "shared/crypto/OpenSslHelper.hxx"
#include <cstddef>
#include <memory>

/**
 * Represents an abstraction of an elliptic curve public key.
 *
 * An object of this class may represents only a public key
 * (the corresponding private key would be impossible to compute).
 */
class EllipticCurvePublicKey
{
public:
    explicit EllipticCurvePublicKey(int nid, const EC_POINT* publicKey);

    /**
     * Tells whether any public key is currently in ownership.
     */
    bool hasPublicKey() const;

    /**
     * Returns the owned public key (or throws an exception if there is not any).
     */
    const EC_POINT* getPublicKey() const;

    /**
     * Retrieves the underlying elliptic curve's finite field.
     */
    const EC_GROUP* getGroup() const;

    /**
     * Computes the size of the underlying elliptic curve's finite field (in bytes).
     */
    std::size_t getFieldSize() const;

    /**
     * Speeds up future point multiplications done using the underlying elliptic curve's
     * finite field (via `::getGroup()`) by precomputing the generator multiplication table.
     */
    void computeMultiplicationTable();

    /**
     * Tells whether this object's public key
     * is the exact same point represented by `rhs`'s public key.
     *
     * Throws an exception if either object does not own a public key.
     */
    bool operator==(const EllipticCurvePublicKey& rhs) const;

    /**
     * Tells whether this object's public key is anything
     * but NOT the exact same point represented by `rhs`'s public key.
     *
     * Throws an exception if either object does not own a public key.
     */
    bool operator!=(const EllipticCurvePublicKey& rhs) const;

private:
    using EcKeyPtr = std::unique_ptr<EC_KEY, void (*)(struct ec_key_st*)>;

    EcKeyPtr mKey;
};

#endif
