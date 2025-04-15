/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurve.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/String.hxx"

#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


class CertificateTest : public testing::Test
{
};

TEST_F(CertificateTest, fromBase64)//NOLINT(readability-function-cognitive-complexity)
{
    const auto certificateBase64 =
        ::ResourceManager::instance().getStringResource("test/cadesBesSignature/certificates/erezept.pem");

    EXPECT_NO_THROW(Certificate::fromBase64(certificateBase64));
    EXPECT_NO_THROW(Certificate::fromBase64(::std::string{"  "} + certificateBase64));
    EXPECT_NO_THROW(Certificate::fromBase64(::std::string{"-- This is a comment\n"} + certificateBase64));
    EXPECT_NO_THROW(Certificate::fromBase64(::std::string{"This is also a comment\n"} + certificateBase64));
    EXPECT_NO_THROW(Certificate::fromBase64(certificateBase64 + ::std::string{"This is another comment"}));

    EXPECT_ANY_THROW(Certificate::fromBase64(certificateBase64.substr(3)));
    EXPECT_ANY_THROW(Certificate::fromBase64(""));
}

TEST_F(CertificateTest, fromPem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto certificatePem =
        ::ResourceManager::instance().getStringResource("test/cadesBesSignature/certificates/erezept.pem");

    EXPECT_NO_THROW(Certificate::fromPem(certificatePem));
    EXPECT_NO_THROW(Certificate::fromPem(::std::string{"  "} + certificatePem));
    EXPECT_NO_THROW(Certificate::fromPem(::std::string{"-- This is a comment\n"} + certificatePem));
    EXPECT_NO_THROW(Certificate::fromPem(::std::string{"This is also a comment\n"} + certificatePem));
    EXPECT_NO_THROW(Certificate::fromPem(certificatePem + ::std::string{"This is another comment"}));

    EXPECT_ANY_THROW(Certificate::fromPem(certificatePem.substr(3)));
    EXPECT_ANY_THROW(Certificate::fromPem(""));
}

TEST_F(CertificateTest, fromBase64Der)//NOLINT(readability-function-cognitive-complexity)
{
    const auto certificateBase64Pem =
        ::ResourceManager::instance().getStringResource("test/cadesBesSignature/certificates/erezept.pem");

    EXPECT_ANY_THROW(Certificate::fromBase64Der(certificateBase64Pem));

    const auto certificateBase64Der =
        ::Base64::encode(::ResourceManager::instance().getStringResource("test/generated_pki/root_ca_ec/ca.der"));

    EXPECT_NO_THROW(Certificate::fromBase64Der(certificateBase64Der));
    EXPECT_ANY_THROW(Certificate::fromBase64Der(""));
}

TEST_F(CertificateTest, fromBinaryDer)//NOLINT(readability-function-cognitive-complexity)
{
    const auto certificateBinaryDer =
        ::ResourceManager::instance().getStringResource("test/generated_pki/root_ca_ec/ca.der");

    EXPECT_NO_THROW(Certificate::fromBinaryDer(certificateBinaryDer));

    const auto certificateBase64Pem =
        ::ResourceManager::instance().getStringResource("test/cadesBesSignature/certificates/erezept.pem");

    EXPECT_ANY_THROW(Certificate::fromBinaryDer(certificateBase64Pem));
    EXPECT_ANY_THROW(Certificate::fromBinaryDer(""));
}

TEST_F(CertificateTest, derRoundtrip)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
        EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromBase64Der(certificate.toBase64Der());

    ASSERT_EQ(
        X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
        X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}


TEST_F(CertificateTest, derRoundtrip2)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
        EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromBase64Der(Base64::encode(certificate.toBinaryDer()));

    ASSERT_EQ(
        X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
        X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}


TEST_F(CertificateTest, pemRoundtrip)
{
    auto certificate = Certificate::createSelfSignedCertificateMock(
            EllipticCurve::BrainpoolP256R1->createKeyPair());

    auto certificate2 = Certificate::fromPem(certificate.toPem());

    ASSERT_EQ(
            X509_NAME_hash(const_cast<X509_NAME*>(certificate2.getSubjectName())),
            X509_NAME_hash(const_cast<X509_NAME*>(certificate.getSubjectName())));
}
