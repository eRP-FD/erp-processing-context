/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_ELLIPTICCURVE_HXX
#define EPA_LIBRARY_CRYPTO_ELLIPTICCURVE_HXX

#include "library/crypto/DiffieHellman.hxx"

#include <memory>

#include "shared/crypto/Certificate.hxx"

namespace epa
{

// GEMREQ-start GS-A_4362
/**
 * Factory for functionality based on an elliptic curve.
 */
class EllipticCurve
{
public:
    virtual ~EllipticCurve() = default;

    static const std::unique_ptr<const EllipticCurve> BrainpoolP256R1;
    static const std::unique_ptr<const EllipticCurve> Prime256v1;

    static const EllipticCurve& getEllipticCurveForNid(const int nid);
    static const EllipticCurve& getEllipticCurveForKey(const EVP_PKEY& key);

    virtual shared_EVP_PKEY createKeyPair() const = 0;

    virtual int getNid() const = 0;
    virtual const std::string& getName() const = 0;
    virtual DiffieHellman createDiffieHellman() const = 0;
    virtual bool isOnCurve(const EVP_PKEY& key) const = 0;

    static int getEcCurveNid(const EVP_PKEY& key);
};
// GEMREQ-end GS-A_4362

} // namespace epa

std::string toString(const X509_NAME* name);


#endif
