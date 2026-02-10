/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/EllipticCurve.hxx"
#include "shared/util/Expect.hxx"

#include <openssl/core_names.h>
#include <openssl/param_build.h>

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
            Fail2("EllipticCurve error at " + location +"\n    " + message, std::runtime_error);
        }
    }

    #define throwIfNot(expression, message) \
        throwIfNot((expression), message, std::string(__FILE__) + ":" + std::to_string(__LINE__))
}


std::unique_ptr<EllipticCurve> EllipticCurve::BrainpoolP256R1 = std::make_unique<EllipticCurveBrainpoolP256R1>();


// ----- EllipticCurveBrainpoolP256R1 ----------------------------------------

shared_EVP_PKEY EllipticCurveBrainpoolP256R1::createKeyPair() const
{
    return shared_EVP_PKEY::make(EVP_EC_gen(SN_brainpoolP256r1));
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
