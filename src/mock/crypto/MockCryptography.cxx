/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/crypto/MockCryptography.hxx"

#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/util/Base64.hxx"

#include "mock/util/MockConfiguration.hxx"


/**
 * This is a simple version of pem to Pkcs8 that does not check that `pem` has the correct format.
 */
SafeString MockCryptography::pemToPkcs8 (const SafeString& pem)
{
    const auto pemView = std::string_view(pem);
    const auto firstNewLine = pemView.find("-----\n");
    const auto lastNewLine = pemView.rfind("\n-----");
    std::string pkcs8 = Base64::decodeToString(
        Base64::cleanupForDecoding(
            std::string(
                pemView.substr(firstNewLine+6, lastNewLine-firstNewLine-6))));
    return SafeString(std::move(pkcs8));
}


const SafeString& MockCryptography::getEciesPublicKeyPem (void)
{
    static SafeString publicEciesKeyPem = MockConfiguration::instance().getSafeStringValue(MockConfigurationKey::MOCK_ECIES_PUBLIC_KEY);
    return publicEciesKeyPem;
}


shared_EVP_PKEY MockCryptography::getEciesPublicKey (void)
{
    static shared_EVP_PKEY publicEciesKey = EllipticCurveUtils::pemToPublicKey(getEciesPublicKeyPem());
    return publicEciesKey;
}


const Certificate& MockCryptography::getEciesPublicKeyCertificate (void)
{
    static auto certificate = Certificate::createSelfSignedCertificateMock(getEciesPrivateKey());
    return certificate;
}


const SafeString& MockCryptography::getEciesPrivateKeyPem (void)
{
    static SafeString privateEciesKeyPem = MockConfiguration::instance().getSafeStringValue(MockConfigurationKey::MOCK_ECIES_PRIVATE_KEY);
    return privateEciesKeyPem;
}


shared_EVP_PKEY MockCryptography::getEciesPrivateKey (void)
{
    static shared_EVP_PKEY privateEciesKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(getEciesPrivateKeyPem());
    return privateEciesKey;
}


shared_EVP_PKEY MockCryptography::getIdpPublicKey (void)
{
    static shared_EVP_PKEY publicIdpKey = EllipticCurveUtils::pemToPublicKey(
        MockConfiguration::instance().getSafeStringValue(MockConfigurationKey::MOCK_IDP_PUBLIC_KEY));
    return publicIdpKey;
}

const Certificate& MockCryptography::getIdpPublicKeyCertificate (void)
{
    static auto certificate = Certificate::createSelfSignedCertificateMock(getIdpPrivateKey());
    return certificate;
}

shared_EVP_PKEY MockCryptography::getIdpPrivateKey (void)
{
    static shared_EVP_PKEY privateIdpKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(
        MockConfiguration::instance().getSafeStringValue(MockConfigurationKey::MOCK_IDP_PRIVATE_KEY));
    return privateIdpKey;
}


const SafeString& MockCryptography::getIdFdSigPrivateKeyPem (void)
{
    static SafeString privateKeyPem = MockConfiguration::instance().getSafeStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_PRIVATE_KEY);
    return privateKeyPem;
}


const SafeString& MockCryptography::getIdFdSigPrivateKeyPkcs8 (void)
{
    static SafeString privateKeyPkcs8 = pemToPkcs8(getIdFdSigPrivateKeyPem());
    return privateKeyPkcs8;
}
