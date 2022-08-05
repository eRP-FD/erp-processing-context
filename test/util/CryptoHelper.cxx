/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/CryptoHelper.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/Base64.hxx"
#include "test/util/TestConfiguration.hxx"

#include "ResourceManager.hxx"
std::string CryptoHelper::toCadesBesSignature(const std::string& content,
                                              const std::optional<fhirtools::Timestamp>& signingTime)
{
    auto pem_str = qesCertificatePem();
    auto cert = Certificate::fromPem(pem_str);
    SafeString pem{std::move(pem_str)};
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(pem);
    CadesBesSignature cadesBesSignature{cert, privKey, content, signingTime};
    return cadesBesSignature.getBase64();
}

std::string CryptoHelper::qesCertificatePem()
{
    const auto& pemFilename = TestConfiguration::instance()
                                  .getOptionalStringValue(TestConfigurationKey::TEST_QES_PEM_FILE_NAME)
                                  .value_or("test/qes.pem");
    return ResourceManager::instance().getStringResource(pemFilename);
}

Certificate CryptoHelper::cHpQes()
{
    return Certificate::fromPem(ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem"));
}

shared_EVP_PKEY CryptoHelper::cHpQesPrv()
{
    auto prcKeyStr = ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem");
    return EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{std::move(prcKeyStr)});
}

Certificate CryptoHelper::cHpQesWansim()
{
    return Certificate::fromPem(ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256_WANSIM.pem"));
}

shared_EVP_PKEY CryptoHelper::cHpQesPrvWansim()
{
    auto prcKeyStr = ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256_WANSIM.prv.pem");
    return EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{std::move(prcKeyStr)});
}
