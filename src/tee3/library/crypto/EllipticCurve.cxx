/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/EllipticCurve.hxx"
#include "library/util/Assert.hxx"

#include <openssl/core_names.h>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

namespace
{
    class NamedEllipticCurve : public EllipticCurve
    {
    public:
        NamedEllipticCurve(int curveNid, const char* curveName)
          : mCurveNid{curveNid},
            mCurveName{curveName}
        {
        }

        // GEMREQ-start GS-A_4368#EllipticCurveKeyBrainpoolP256R1
        shared_EVP_PKEY createKeyPair() const override
        {
            auto pkey = shared_EVP_PKEY::make();

            auto key = shared_EC_KEY::make(EC_KEY_new_by_curve_name(mCurveNid));
            AssertOpenSsl(key.isSet())
                << "could not create new EC key on " << mCurveName << " curve";
            EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
            int status = EC_KEY_generate_key(key);
            AssertOpenSsl(status == 1) << "generating EC key failed";
            status = EVP_PKEY_set1_EC_KEY(pkey, key);
            AssertOpenSsl(status == 1) << "generating EVP key from EC key failed";

            return pkey;
        }
        // GEMREQ-end GS-A_4368#EllipticCurveKeyBrainpoolP256R1

        int getNid() const override
        {
            return mCurveNid;
        }

        const std::string& getName() const override
        {
            return mCurveName;
        }

        DiffieHellman createDiffieHellman() const override
        {
            return DiffieHellman(
                NID_X9_62_id_ecPublicKey, // identical to EVP_PKEY_EC
                mCurveNid);
        }

        bool isOnCurve(const EVP_PKEY& key) const override
        {
            const auto* eckey = EVP_PKEY_get0_EC_KEY(&key);
            if (eckey == nullptr)
                return false;
            const auto* group = EC_KEY_get0_group(eckey);
            if (group == nullptr)
                return false;
            auto name = EC_GROUP_get_curve_name(group);
            return name == mCurveNid;
        }

    private:
        int mCurveNid;
        std::string mCurveName;
    };
}


const std::unique_ptr<const EllipticCurve> EllipticCurve::BrainpoolP256R1 =
    std::make_unique<NamedEllipticCurve>(NID_brainpoolP256r1, SN_brainpoolP256r1);
const std::unique_ptr<const EllipticCurve> EllipticCurve::Prime256v1 =
    std::make_unique<NamedEllipticCurve>(NID_X9_62_prime256v1, SN_X9_62_prime256v1);


const EllipticCurve& EllipticCurve::getEllipticCurveForNid(const int nid)
{
    for (const EllipticCurve* curve : {BrainpoolP256R1.get(), Prime256v1.get()})
    {
        if (nid == curve->getNid())
            return *curve;
    }
    Failure() << "elliptic curve with NID " << nid << " not supported";
}


const EllipticCurve& EllipticCurve::getEllipticCurveForKey(const EVP_PKEY& key)
{
    return getEllipticCurveForNid(getEcCurveNid(key));
}


int EllipticCurve::getEcCurveNid(const EVP_PKEY& key)
{
    Assert(EVP_PKEY_get_base_id(&key) == EVP_PKEY_EC) << "The given key is not an EC key";

    OSSL_PARAM params[2];
    char curveName[256];
    params[0] =
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, curveName, sizeof(curveName));
    params[1] = OSSL_PARAM_construct_end();

    Assert(EVP_PKEY_get_params(&key, params) == 1) << "Failed to get EC group from EVP_PKEY";
    int nid = OBJ_txt2nid(curveName);
    Assert(nid != NID_undef) << "Failed to get curve NID from EVP_PKEY";
    return nid;
}

} // namespace epa

std::string toString(const X509_NAME* name)
{
    using namespace epa;
    auto mem = shared_BIO::make();
    const int status = X509_NAME_print(mem.get(), name, 0);
    AssertOpenSsl(status == 1) << "serialization of certificate failed";
    return bioToString(mem.get());
}
