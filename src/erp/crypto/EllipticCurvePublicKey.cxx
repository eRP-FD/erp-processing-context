/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/EllipticCurvePublicKey.hxx"

#include <stdexcept>

#include "erp/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "erp/util/OpenSsl.hxx"


namespace
{
    using EcGroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;
}


EllipticCurvePublicKey::EllipticCurvePublicKey(int nid, const EC_POINT* publicKey)
: mKey{EC_KEY_new_by_curve_name(nid), EC_KEY_free}
{
    OpenSslExpect(mKey != nullptr, "EllipticCurvePublicKey: initialization failed");

    Expect(publicKey != nullptr, "EllipticCurvePublicKey: publicKey must not be null");
    OpenSslExpect(1 == EC_POINT_is_on_curve(getGroup(), publicKey, nullptr),
                  "EllipticCurvePublicKey: public key not on expected elliptic curve");

    OpenSslExpect(1 == EC_KEY_set_public_key(mKey.get(), publicKey),
                  "EllipticCurvePublicKey: setting of public key failed");
}


bool EllipticCurvePublicKey::hasPublicKey() const
{
    return EC_KEY_get0_public_key(mKey.get()) != nullptr;
}


const EC_POINT* EllipticCurvePublicKey::getPublicKey() const
{
    const auto* publicKeyResult = EC_KEY_get0_public_key(mKey.get());
    if (!publicKeyResult)
    {
        Fail2("No public key stored", std::logic_error);
    }

    return publicKeyResult;
}


const EC_GROUP* EllipticCurvePublicKey::getGroup() const
{
    const auto* groupResult = EC_KEY_get0_group(mKey.get());
    OpenSslExpect(groupResult != nullptr,
                  "EllipticCurvePublicKey: group retrieval failed");

    return groupResult;
}


std::size_t EllipticCurvePublicKey::getFieldSize() const
{
    const auto size = (EC_GROUP_get_degree(getGroup()) + 7) / 8;
    return gsl::narrow<std::size_t>(size);
}


void EllipticCurvePublicKey::computeMultiplicationTable()
{
    EcGroupPtr groupCopy{EC_GROUP_dup(getGroup()), EC_GROUP_free};
    OpenSslExpect(nullptr != groupCopy,
                  "EllipticCurvePublicKey: group duplication failed");

    OpenSslExpect(EC_GROUP_precompute_mult(groupCopy.get(), nullptr),
                  "EllipticCurvePublicKey: precomputation failed");
    OpenSslExpect((EC_KEY_set_group(mKey.get(), groupCopy.get())),
                  "EllipticCurvePublicKey: group setting failed");
}


bool EllipticCurvePublicKey::operator==(const EllipticCurvePublicKey& rhs) const
{
    OpenSslExpect(hasPublicKey(),
                  "EllipticCurvePublicKey: this object has no public key by comparing");
    OpenSslExpect(rhs.hasPublicKey(),
                  "EllipticCurvePublicKey: provided object has no public key by comparing");

    auto comparisonResult = EC_POINT_cmp(getGroup(), getPublicKey(), rhs.getPublicKey(), nullptr);
    OpenSslExpect(0 <= comparisonResult, "EllipticCurvePublicKey: error by comparing");

    return 0 == comparisonResult;
}


bool EllipticCurvePublicKey::operator!=(const EllipticCurvePublicKey& rhs) const
{
    return !(*this == rhs);
}
