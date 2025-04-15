/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/EllipticCurveKey.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <stdexcept>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

namespace
{
    using EcGroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;

    /**
     * Throws an exception unless the given parameter has the expected value of 1.
     *
     * Will be refactored via the introduction of more specific exception types.
     */
    void throwException(int operationResult)
    {
        if (1 != operationResult)
        {
            throw std::runtime_error{"EllipticCurveKey error"};
        }
    }
}


// GEMREQ-start A_17874#ellipticCurveKey, A_17875#ellipticCurveKey
EllipticCurveKey::EllipticCurveKey(int nid)
  : mKey{EC_KEY_new_by_curve_name(nid), &EC_KEY_free}
{
    throwException(mKey != nullptr);
}
// GEMREQ-end A_17874#ellipticCurveKey, A_17875#ellipticCurveKey

bool EllipticCurveKey::hasPublicKey() const
{
    return EC_KEY_get0_public_key(mKey.get()) != nullptr;
}


const EC_POINT* EllipticCurveKey::getPublicKey() const
{
    const auto* publicKeyResult = EC_KEY_get0_public_key(mKey.get());
    if (! publicKeyResult)
    {
        throw std::logic_error{"No public key stored"};
    }

    return publicKeyResult;
}


void EllipticCurveKey::setPublicKey(const EC_POINT* publicKey)
{
    throwException(EC_KEY_set_public_key(mKey.get(), publicKey));

    clearPrivateKey();
}


void EllipticCurveKey::clearPublicKey()
{
    if (hasPublicKey())
    {
        throwException(0 == EC_KEY_set_public_key(mKey.get(), nullptr));
    }
}


bool EllipticCurveKey::hasPrivateKey() const
{
    return EC_KEY_get0_private_key(mKey.get()) != nullptr;
}


const BIGNUM* EllipticCurveKey::getPrivateKey() const
{
    const auto* privateKeyResult = EC_KEY_get0_private_key(mKey.get());
    if (! privateKeyResult)
    {
        throw std::logic_error{"No private key stored"};
    }

    return privateKeyResult;
}


void EllipticCurveKey::setKeyPair(const BIGNUM* privateKey, const EC_POINT* publicKey)
{
    throwException(EC_KEY_set_private_key(mKey.get(), privateKey));
    throwException(EC_KEY_set_public_key(mKey.get(), publicKey));
}


void EllipticCurveKey::clearPrivateKey()
{
    if (hasPrivateKey())
    {
        throwException(0 == EC_KEY_set_private_key(mKey.get(), nullptr));
    }
}


// GEMREQ-start GS-A_4368#generateEllipticCurveKey
void EllipticCurveKey::generate()
{
    throwException(EC_KEY_generate_key(mKey.get()));
}
// GEMREQ-end GS-A_4368#generateEllipticCurveKey


const EC_GROUP* EllipticCurveKey::getGroup() const
{
    const auto* groupResult = EC_KEY_get0_group(mKey.get());
    throwException(groupResult != nullptr);

    return groupResult;
}


std::size_t EllipticCurveKey::getFieldSize() const
{
    const auto size = (EC_GROUP_get_degree(getGroup()) + 7) / 8;
    return gsl::narrow<std::size_t>(size);
}


EC_KEY* EllipticCurveKey::getNativeHandle() const
{
    return mKey.get();
}


void EllipticCurveKey::computeMultiplicationTable()
{
    EcGroupPtr groupCopy{EC_GROUP_dup(getGroup()), &EC_GROUP_free};
    throwException(nullptr != groupCopy);

    throwException(EC_GROUP_precompute_mult(groupCopy.get(), nullptr));
    throwException(EC_KEY_set_group(mKey.get(), groupCopy.get()));
}


bool EllipticCurveKey::operator==(const EllipticCurveKey& rhs) const
{
    throwException(hasPublicKey());
    throwException(rhs.hasPublicKey());

    auto comparisonResult = EC_POINT_cmp(getGroup(), getPublicKey(), rhs.getPublicKey(), nullptr);
    throwException(0 <= comparisonResult);

    return 0 == comparisonResult;
}


bool EllipticCurveKey::operator!=(const EllipticCurveKey& rhs) const
{
    return ! (*this == rhs);
}

} // namespace epa
