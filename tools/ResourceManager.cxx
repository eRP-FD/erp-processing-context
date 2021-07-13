#include "ResourceManager.hxx"

#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/FileHelper.hxx"
#include "tool_config.hxx"

#include <initializer_list>
#include <rapidjson/error/en.h>
#include <stdexcept>

ResourceManager& ResourceManager::instance()
{
    static ResourceManager theInstance;
    return theInstance;
}

ResourceManager::ResourceManager() = default;

ResourceManager::~ResourceManager() noexcept = default;


std::filesystem::path ResourceManager::getAbsoluteFilename(const std::filesystem::path& resourceFile)
{
    const std::initializer_list<std::filesystem::path> tryFiles
    {
            ToolConfig::RESOURCE_BINARY_DIR / resourceFile,
            ToolConfig::RESOURCE_SOURCE_DIR / resourceFile
    };
    for (const auto& file : tryFiles)
    {
        if (exists(file))
        {
            return file;
        }
    }
    using namespace std::string_literals;
    throw std::runtime_error("Resource not found: "s + resourceFile.generic_string());
}

const std::string& ResourceManager::getStringResource(const std::string_view& resourceFile)
{
    auto [iterator, inserted] = stringResources.emplace(std::string{resourceFile}, std::string{});
    if (inserted)
    {
        auto absoluteFilename = getAbsoluteFilename(resourceFile);
        iterator->second = FileHelper::readFileAsString(absoluteFilename.string());

    }
    return iterator->second;
}


const rapidjson::Document& ResourceManager::getJsonResource(const std::string_view& resourceFile)
{
    auto [iterator, inserted] = jsonResources.emplace(std::string{resourceFile}, rapidjson::Document{});
    if (inserted)
    {
        const auto& jsonString = getStringResource(resourceFile);
        rapidjson::Document doc;
        rapidjson::ParseResult result = doc.Parse(jsonString);
        if (result.IsError())
        {
            using std::min;
            std::string nearStr = jsonString.substr(
                result.Offset(),
                min(jsonString.size() - result.Offset(), static_cast<size_t>(10)));
            std::ostringstream msg;
            msg << "JSON Parse Error in " << resourceFile << " '"
                << rapidjson::GetParseError_En(result.Code()) << "' near: " << nearStr;
            LOG(ERROR) << msg.str();
            throw std::runtime_error(msg.str());
        }
        iterator->second = std::move(doc);
    }
    return iterator->second;
}
