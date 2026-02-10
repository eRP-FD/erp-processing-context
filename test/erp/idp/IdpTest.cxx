/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/idp/Idp.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/crypto/EllipticCurve.hxx"

#include <gtest/gtest.h>


class IdpTest : public testing::Test
{
public:
    void verifyKeyPair(const shared_EVP_PKEY& publicKey, const shared_EVP_PKEY& privateKey)
    {
        ASSERT_EQ(EVP_PKEY_eq(privateKey, publicKey), 1);
    }
};


TEST_F(IdpTest, setCertificate)//NOLINT(readability-function-cognitive-complexity)
{
    Idp idp;

    // Initially the certificate is not set.
    ASSERT_ANY_THROW(idp.getCertificate());

    // Set an arbitrary certificate.
    idp.setCertificate(Certificate::createSelfSignedCertificateMock(MockCryptography::getEciesPrivateKey()));

    ASSERT_NO_THROW(idp.getCertificate());
}


TEST_F(IdpTest, resetCertificate)
{
    Idp idp;
    idp.setCertificate(Certificate::createSelfSignedCertificateMock(MockCryptography::getEciesPrivateKey()));

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
    verifyKeyPair(publicKey, privateKey);
}


// Verify that the mock ecies key pair is consistent.
TEST_F(IdpTest, mockEciesKeyPair)
{
    const auto privateKey = MockCryptography::getEciesPrivateKey();
    const auto publicKey = MockCryptography::getEciesPublicKey();

    verifyKeyPair(publicKey, privateKey);
}

TEST_F(IdpTest, ephemeralKeyPair)
{
    const auto privateKey = EllipticCurve::BrainpoolP256R1->createKeyPair();
    const auto publicKey = EllipticCurveUtils::pemToPublicKey(SafeString(EllipticCurveUtils::publicKeyToPem(privateKey)));

    verifyKeyPair(publicKey, privateKey);
}


TEST_F(IdpTest, setSecondaryCertificate)
{
    Idp idp;

    // Initially the secondary certificate is not set.
    ASSERT_FALSE(idp.getSecondaryCertificate().has_value());

    // Set an arbitrary certificate.
    idp.setSecondaryCertificate(Certificate::createSelfSignedCertificateMock(MockCryptography::getEciesPrivateKey()));

    ASSERT_TRUE(idp.getSecondaryCertificate().has_value());
    idp.resetSecondaryCertificate();
    ASSERT_FALSE(idp.getSecondaryCertificate().has_value());
}
