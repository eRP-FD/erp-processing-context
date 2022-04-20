/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "ResourceManager.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/util/TestConfiguration.hxx"

#include <rapidjson/error/en.h>
#include <initializer_list>
#include <stdexcept>

#include "tool_config.hxx"

ResourceManager& ResourceManager::instance()
{
    static ResourceManager theInstance;
    return theInstance;
}

ResourceManager::ResourceManager() = default;

ResourceManager::~ResourceManager() noexcept = default;


std::filesystem::path ResourceManager::getAbsoluteFilename(const std::filesystem::path& resourceFile)
{
    if (resourceFile.is_absolute())
    {
        return resourceFile;
    }
    const std::initializer_list<std::filesystem::path> tryFiles
    {
            ToolConfig::RESOURCE_BINARY_DIR / resourceFile,
            ToolConfig::RESOURCE_SOURCE_DIR / resourceFile,
            std::filesystem::path(
                TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_RESOURCE_MANAGER_PATH)) /
                resourceFile};
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
    {
        std::shared_lock slock{mutex};
        auto res = stringResources.find(std::string{resourceFile});
        if (res != stringResources.end())
        {
            return res->second;
        }
    }
    std::unique_lock lock{mutex};
    return getStringResource(resourceFile, lock);
}

const std::string & ResourceManager::getStringResource(const std::string_view& resourceFile, std::unique_lock<std::shared_mutex>&)
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
    {
        std::shared_lock slock{mutex};
        auto res = jsonResources.find(std::string{resourceFile});
        if (res != jsonResources.end())
        {
            return res->second;
        }
    }
    std::unique_lock lock{mutex};
    auto [iterator, inserted] = jsonResources.emplace(std::string{resourceFile}, rapidjson::Document{});
    if (inserted)
    {
        const auto& jsonString = getStringResource(resourceFile, lock);
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

std::string_view ResourceManager::resourceBinaryDir()
{
    return ToolConfig::RESOURCE_BINARY_DIR;
}
