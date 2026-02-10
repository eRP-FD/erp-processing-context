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
            return shared_EVP_PKEY::make(EVP_EC_gen(OBJ_nid2sn(mCurveNid)));
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
            if (! EVP_PKEY_is_a(&key, "EC"))
            {
                return false;
            }

            std::array<char, 64> groupName{};
            size_t len = 0;
            if (EVP_PKEY_get_group_name(&key, groupName.data(), groupName.size(), &len) != 1)
            {
                return false;
            }
            const auto curveId = OBJ_txt2nid(groupName.data());
            return curveId == mCurveNid;
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
    const int status = X509_NAME_print_ex(mem.get(), name, 0, XN_FLAG_ONELINE);
    AssertOpenSsl(status != -1) << "serialization of certificate failed";
    return bioToString(mem.get());
}
