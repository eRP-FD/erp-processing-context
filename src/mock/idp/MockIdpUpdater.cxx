/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockIdpUpdater.hxx"
#include "shared/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/util/MockConfiguration.hxx"

#include <regex>

std::string MockIdpUpdater::doDownloadWellknown(void)
{
    return FileHelper::readFileAsString(
        MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH) +
        "/../tsl/X509Certificate/idpResponseJwk.txt");
}


std::string MockIdpUpdater::doDownloadDiscovery(const UrlHelper::UrlParts&)
{
    const auto idpResponse = FileHelper::readFileAsString(
        MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH) +
        "/../tsl/X509Certificate/idpResponse.json");
    const auto idpCertificate = Certificate::fromPem(FileHelper::readFileAsString(
        MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH) +
        "/../tsl/X509Certificate/IDP-Wansim.pem"));
    return std::regex_replace(idpResponse, std::regex{"###CERTIFICATE##"}, idpCertificate.toBase64Der());
}
