/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/idp/Idp.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "erp/crypto/EllipticCurve.hxx"

#include <gtest/gtest.h>


class IdpTest : public testing::Test
{
public:
    void verifyKeyPair (const shared_EVP_PKEY& publicKey, const shared_EVP_PKEY& privateKey, const std::string& dataToSign)
    {
        auto* privateEcKey = EVP_PKEY_get0_EC_KEY(privateKey.removeConst());
        ECDSA_SIG* signature = ECDSA_do_sign(reinterpret_cast<const unsigned char*>(dataToSign.data()), gsl::narrow_cast<int>(dataToSign.size()), privateEcKey);

        auto* publicEcKey = EVP_PKEY_get0_EC_KEY(publicKey.removeConst());
        const int status = ECDSA_do_verify(reinterpret_cast<const unsigned char*>(dataToSign.data()), gsl::narrow_cast<int>(dataToSign.size()), signature, publicEcKey);
        ASSERT_EQ(status, 1);

        ECDSA_SIG_free(signature);
    }
};


TEST_F(IdpTest, setCertificate)//NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // Set an arbitrary certificate.
    idp.setCertificate(Certificate::build().withPublicKey(MockCryptography::getEciesPublicKey()).build());

    ASSERT_NO_THROW(idp.getCertificate());
}


// TODO: reenable once the workaround for ERP-5548 is no longer needed.
TEST_F(IdpTest, DISABLED_resetCertificate)//NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;
    idp.setCertificate(Certificate::build().withPublicKey(MockCryptography::getEciesPublicKey()).build());

    ASSERT_NO_THROW(idp.getCertificate());

    // Reset the certificate ...
    idp.resetCertificate();

    // .. and verify that it is no longer available.
    ASSERT_ANY_THROW(idp.getCertificate());
}


// Verify that the mock idp key pair is consistent.
TEST_F(IdpTest, mockIdpKeyPair)
{
    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();

    const std::string dataToSign = "data to sign";
    verifyKeyPair(publicKey, privateKey, dataToSign);
}


// Verify that the mock ecies key pair is consistent.
TEST_F(IdpTest, mockEciesKeyPair)
{
    const auto privateKey = MockCryptography::getEciesPrivateKey();
    const auto publicKey = MockCryptography::getEciesPublicKey();

    const std::string dataToSign = "data to sign";
    verifyKeyPair(publicKey, privateKey, dataToSign);
}

TEST_F(IdpTest, ephemeralKeyPair)
{
    const auto privateKey = EllipticCurve::BrainpoolP256R1->createKeyPair();
    const auto publicKey = EllipticCurveUtils::pemToPublicKey(SafeString(EllipticCurveUtils::publicKeyToPem(privateKey)));

    const std::string dataToSign = "data to sign";

    verifyKeyPair(publicKey, privateKey, dataToSign);
}
