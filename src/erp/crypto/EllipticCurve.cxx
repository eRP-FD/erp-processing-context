/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/EllipticCurve.hxx"


class EllipticCurveBrainpoolP256R1
    : public EllipticCurve
{
public:
    shared_EVP_PKEY createKeyPair (void) const override;
    shared_EC_GROUP getEcGroup (void) const override;
    DiffieHellman createDiffieHellman (void) const override;
};


namespace {
    void throwIfNot (const bool expectedTrue, const std::string& message, const std::string& location)
    {
        if ( ! expectedTrue)
        {
            showAllOpenSslErrors();
            throw std::runtime_error("EllipticCurve error at " + location +"\n    " + message);
        }
    }

    #define throwIfNot(expression, message) \
        throwIfNot((expression), message, std::string(__FILE__) + ":" + std::to_string(__LINE__))
}


std::unique_ptr<EllipticCurve> EllipticCurve::BrainpoolP256R1 = std::make_unique<EllipticCurveBrainpoolP256R1>();


// ----- EllipticCurveBrainpoolP256R1 ----------------------------------------

shared_EVP_PKEY EllipticCurveBrainpoolP256R1::createKeyPair (void) const
{
    auto pkey = shared_EVP_PKEY::make();

    auto key = shared_EC_KEY::make(EC_KEY_new_by_curve_name(NID));
    throwIfNot(
        key.isSet(),
        "could not create new EC key on brainpoolP256R1 curve");
    EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
    int status = EC_KEY_generate_key(key);
    throwIfNot(
        status == 1,
        "generating EC key failed");
    status = EVP_PKEY_set1_EC_KEY(pkey, key);
    throwIfNot(
        status == 1,
        "generating EVP key from EC key failed");

    return pkey;
}


shared_EC_GROUP EllipticCurveBrainpoolP256R1::getEcGroup (void) const
{
    auto group = shared_EC_GROUP::make(EC_GROUP_new_by_curve_name(NID_brainpoolP256r1));
    throwIfNot(
        group.isSet(),
        "could not create EC group for brainpoolP256R1 curve");
    return group;
}


DiffieHellman EllipticCurveBrainpoolP256R1::createDiffieHellman (void) const
{
    return {};
}
