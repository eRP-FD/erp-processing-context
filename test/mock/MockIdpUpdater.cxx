#include "test/mock/MockIdpUpdater.hxx"

#include "erp/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "tools/jwt/JwtBuilder.hxx"

#include <test_config.h>


std::string MockIdpUpdater::doDownloadWellknown (void)
{
    return FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
}


std::string MockIdpUpdater::doDownloadDiscovery (const UrlHelper::UrlParts&)
{
    return FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponse.json");
}
