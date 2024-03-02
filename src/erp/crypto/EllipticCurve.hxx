/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_CRYPTO_ELLIPTICCURVE_HXX
#define E_LIBRARY_CRYPTO_ELLIPTICCURVE_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/DiffieHellman.hxx"

#include <memory>


/**
 * Factory for functionality based on an elliptic curve.
 */
class EllipticCurve
{
public:
    virtual ~EllipticCurve() = default;

    static std::unique_ptr<EllipticCurve> BrainpoolP256R1;

    static constexpr int NID = NID_brainpoolP256r1;

    // Number of bytes, after padding, of both x and y coordinates of public keys on an elliptic curve.
    static constexpr size_t KeyCoordinateLength = 32;

    virtual shared_EVP_PKEY createKeyPair (void) const = 0;

    virtual shared_EC_GROUP getEcGroup (void) const = 0;
    virtual DiffieHellman createDiffieHellman (void) const = 0;
};

#endif
