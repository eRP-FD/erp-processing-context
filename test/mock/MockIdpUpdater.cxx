/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockIdpUpdater.hxx"

#include "erp/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "tools/jwt/JwtBuilder.hxx"
#include "tools/ResourceManager.hxx"
#include "test_config.h"

#include <regex>

std::string MockIdpUpdater::doDownloadWellknown (void)
{
    return FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
}


std::string MockIdpUpdater::doDownloadDiscovery (const UrlHelper::UrlParts&)
{
    auto& resMgr = ResourceManager::instance();
    const auto idpResponse = resMgr.getStringResource("test/tsl/X509Certificate/idpResponse.json");
    auto idpCertificate = Certificate::fromPemString(
        resMgr.getStringResource("test/tsl/X509Certificate/IDP-Wansim.pem"));
    return std::regex_replace(idpResponse, std::regex{"###CERTIFICATE##"}, idpCertificate.toDerBase64String());
}

