#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/GLogConfiguration.hxx"
#include "erp/util/SafeString.hxx"
#include "JwtBuilder.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "ResourceManager.hxx"

#include <rapidjson/error/en.h>
#include <sstream>

std::string usage(const char* argv0)
{
    std::ostringstream oss;
    oss << "Usage: " << argv0 << " claimfile\n\n"
        << "claimfile    file containing claim to sign\n";
    return oss.str();
}


int main(int argc, char* argv[])
{
    using std::min;
    GLogConfiguration::init_logging("jwt");
    if (argc != 2)
    {
        LOG(ERROR) << usage(argv[0]);
        return EXIT_FAILURE;
    }
    try {
        SafeString idpIdPem{FileHelper::readFileAsString(ResourceManager::getAbsoluteFilename("/test/jwt/idp_id"))};
        auto key = EllipticCurveUtils::pemToPrivatePublicKeyPair(idpIdPem);
        auto claim = FileHelper::readFileAsString(argv[1]);
        rapidjson::Document doc;
        rapidjson::ParseResult result = doc.Parse(claim);
        if (result.IsError())
        {
            std::string nearStr = claim.substr(result.Offset(), min(claim.size() - result.Offset(), static_cast<size_t>(10)));
            LOG(ERROR) << "JSON Parse Error '" << rapidjson::GetParseError_En(result.Code()) << "' near: " << nearStr;
            return EXIT_FAILURE;
        }
        JwtBuilder builder(key);
        std::cout << builder.getJWT(doc).serialize() << std::endl;
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << ex.what();
        showAllOpenSslErrors();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


