/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurve.hxx"
#include "erp/util/Base64.hxx"


class CertificateTest : public testing::Test
{
};


TEST_F(CertificateTest, derRoundtrip)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
        EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromDerBase64String(certificate.toDerBase64String());

    ASSERT_EQ(
        X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
        X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}


TEST_F(CertificateTest, derRoundtrip2)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
        EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromDerBase64String(Base64::encode(certificate.toDerString()));

    ASSERT_EQ(
        X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
        X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}


TEST_F(CertificateTest, pemRoundtrip)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
            EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromPemString(certificate.toPemString());

    ASSERT_EQ(
            X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
            X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}
