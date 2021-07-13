#ifndef ERP_PROCESSING_CONTEXT_TOOLS_RESOURCEMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_TOOLS_RESOURCEMANAGER_HXX

#include "erp/crypto/OpenSslHelper.hxx"

#include <filesystem>
#include <map>
#include <memory>
#include <rapidjson/document.h>
#include <string>
#include <string_view>

class ResourceManager
{
public:
    static ResourceManager& instance();

    static std::filesystem::path getAbsoluteFilename(const std::filesystem::path& resourceFile);

    const std::string& getStringResource(const std::string_view& resourceFile);
    const rapidjson::Document& getJsonResource(const std::string_view& resourceFile);

    virtual ~ResourceManager() noexcept;


    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator =(const ResourceManager&) = delete;
    ResourceManager& operator =(ResourceManager&&) = delete;
private:
    ResourceManager();

    std::map<std::string, std::string> stringResources;
    std::map<std::string, shared_EVP_PKEY> publicKeyResources;
    std::map<std::string, rapidjson::Document> jsonResources;
};

#endif// ERP_PROCESSING_CONTEXT_TOOLS_RESOURCEMANAGER_HXX
