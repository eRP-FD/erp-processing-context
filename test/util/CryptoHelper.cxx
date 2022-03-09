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
#include "tools/ResourceManager.hxx"
std::string CryptoHelper::toCadesBesSignature(const std::string& content,
                                              const std::optional<model::Timestamp>& signingTime)
{
    auto pem_str = qesCertificatePem();
    auto cert = Certificate::fromPem(pem_str);
    SafeString pem{std::move(pem_str)};
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(pem);
    CadesBesSignature cadesBesSignature{cert, privKey, content, signingTime};
    return Base64::encode(cadesBesSignature.get());
}

std::string CryptoHelper::qesCertificatePem()
{
    const auto& pemFilename = TestConfiguration::instance()
                                  .getOptionalStringValue(TestConfigurationKey::TEST_QES_PEM_FILE_NAME)
                                  .value_or("test/qes.pem");
    return ResourceManager::instance().getStringResource(pemFilename);
}
