/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ResourceManager.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/InvalidConfigurationException.hxx"
#include "shared/util/TLog.hxx"
#include "test/util/TestConfiguration.hxx"

#include <rapidjson/error/en.h>
#include <list>
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
    std::list<std::filesystem::path> tryFiles
    {
            ToolConfig::RESOURCE_BINARY_DIR / resourceFile,
            ToolConfig::RESOURCE_SOURCE_DIR / resourceFile,
            ToolConfig::RESOURCE_OUTPUT_DIR / resourceFile,
    };
    const auto testResourceManagerPath = TestConfiguration::instance().getOptionalStringValue(
        TestConfigurationKey::TEST_RESOURCE_MANAGER_PATH, std::string{});
    if (!testResourceManagerPath.empty())
    {
        tryFiles.emplace_front(std::filesystem::path{testResourceManagerPath} / resourceFile);
    }
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


rapidjson::Document ResourceManager::toJsonDoc(const std::string &jsonString) {
    rapidjson::Document doc;
    rapidjson::ParseResult result = doc.Parse(jsonString);
    if (result.IsError())
    {
        using std::min;
        std::string nearStr = jsonString.substr(
            result.Offset(),
            min(jsonString.size() - result.Offset(), static_cast<size_t>(10)));
        std::ostringstream msg;
        msg << "JSON Parse Error in " << jsonString << " '"
                << rapidjson::GetParseError_En(result.Code()) << "' near: " << nearStr;
        TLOG(ERROR) << msg.str();
        throw std::runtime_error(msg.str());
    }
    return doc;
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
        iterator->second = toJsonDoc(jsonString);
    }
    return iterator->second;
}

std::string_view ResourceManager::resourceBinaryDir()
{
    return ToolConfig::RESOURCE_BINARY_DIR;
}
