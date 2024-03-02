/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/CryptoHelper.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/Base64.hxx"
#include "test/util/TestConfiguration.hxx"

#include "ResourceManager.hxx"

std::string CryptoHelper::createCadesBesSignature(const std::string& pem_str,
                                                  const std::string& content,
                                                  const std::optional<model::Timestamp>& signingTime)
{
    auto cert = Certificate::fromPem(pem_str);
    SafeString pem{pem_str.c_str()};
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(pem);
    CadesBesSignature cadesBesSignature{cert, privKey, content, signingTime};
    return cadesBesSignature.getBase64();
}

std::string CryptoHelper::toCadesBesSignature(const std::string& content,
                                              const std::optional<model::Timestamp>& signingTime)
{
    auto pem_str = qesCertificatePem();
    return createCadesBesSignature(pem_str, content, signingTime);
}

std::string CryptoHelper::toCadesBesSignature(const std::string& content,
                                              const std::string& cert_pem_filename,
                                              const std::optional<model::Timestamp>& signingTime)
{
    auto pem_str = qesCertificatePem(cert_pem_filename);
    return createCadesBesSignature(pem_str, content, signingTime);
}

std::string CryptoHelper::qesCertificatePem(const std::optional<std::string>& cert_pem_filename)
{
    const auto& pemFilename = cert_pem_filename.value_or(TestConfiguration::instance()
                                  .getOptionalStringValue(TestConfigurationKey::TEST_QES_PEM_FILE_NAME)
                                  .value_or("test/qes.pem"));
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

Certificate CryptoHelper::cQesG0()
{
    return Certificate::fromPem(ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/QES-G0-Certificate.pem"));
}

shared_EVP_PKEY CryptoHelper::cQesG0Prv()
{
    auto prcKeyStr = ResourceManager::instance().getStringResource(
        "test/tsl/X509Certificate/QES-G0-Certificate.prv.pem");
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
