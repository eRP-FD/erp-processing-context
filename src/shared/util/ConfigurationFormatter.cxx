/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ConfigurationFormatter.hxx"
#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
// #include "shared/crypto/Sha256.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/String.hxx"

#include <filesystem>
#include <fstream>
#include <functional>
#include <locale>
#include <set>
#include <string>
#include <string_view>
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>

namespace {
using Type = ConfigurationKeyType;
using Flags = ConfigurationKeyFlags;
constexpr std::string_view filePrefix = "file://";
constexpr std::string_view pemPrefix = "pem:";

bool hasPrivateKeyHeader(std::string_view content)
{
    return content.find("-----BEGIN PRIVATE KEY-----") != std::string::npos;
}

}

ConfigurationFormatter::ConfigurationFormatter()
    : mConfNames{std::make_unique<OpsConfigKeyNames>()}
{
}
ConfigurationFormatter::~ConfigurationFormatter() = default;

std::string ConfigurationFormatter::formatAsJson(const Configuration& config, ConfigurationKeyFlags flags)
{
    rapidjson::Document document;
    // to determine unused "ERP_" variables, collect all with the prefix
    // and on each configuration value, we will remove it from this list once
    // we have seen it
    std::set<std::string, std::less<>> erpEnvVariables;
    for (char** current = environ; *current; current++)
    {
        const auto varName = String::split(*current, '=').at(0);
        if (varName.starts_with("ERP_"))
        {
            erpEnvVariables.insert(varName);
        }
    }
    erpEnvVariables.erase(std::string{ConfigurationBase::ServerHostEnvVar});
    erpEnvVariables.erase(std::string{ConfigurationBase::ServerPortEnvVar});
    document.SetObject();
    for (const auto& confKey : mConfNames->allKeys())
    {
        const auto confOption = mConfNames->strings(confKey);
        if ((confOption.flags & flags) == Flags::none)
        {
            continue;
        }
        addConfigOption(document, config, confKey);
        if (auto it = erpEnvVariables.find(confOption.environmentVariable); it != erpEnvVariables.end())
        {
            erpEnvVariables.erase(it);
        }
    }

    const rapidjson::Pointer unusedVarsPointer("/unusedVariables");
    auto& unusedVarsArray = unusedVarsPointer.Create(document).SetArray();
    for (const auto& envVar : erpEnvVariables)
    {
        unusedVarsArray.PushBack(rapidjson::Value(envVar, document.GetAllocator()), document.GetAllocator());
    }
    if ((Flags::categoryRuntime & flags) != Flags{})
    {
        appendRuntimeConfiguration(document);
    }
    if ((Flags::categoryFhirPackages & flags) != Flags{})
    {
        appendFhirPackagesConfiguration(document);
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    document.Accept(writer);
    return buffer.GetString();
}
void ConfigurationFormatter::addConfigOption(rapidjson::Document& document, const Configuration& config,
                                             ConfigurationKey confKey)
{
    const auto confOption = mConfNames->strings(confKey);
    addConfigOption(document, config, confKey, confOption);
}

void ConfigurationFormatter::addConfigOption(rapidjson::Document& document, const Configuration& config,
                                             ConfigurationKey confKey, const KeyData& confOption)
{
    std::string value;
    std::string defaultValue;

    const std::string& key = baseJsonPath(confOption);
    if (confOption.hasFlags(Flags::array))
    {
        value = String::join(config.getOptionalArray(confKey), ";");
        defaultValue = String::join(config.getOptionalArrayFromJson(confKey), ";");
    }
    else
    {
        value = config.getOptionalStringValue(confKey, "<unset>");
        defaultValue = config.getOptionalStringFromJson(confKey).value_or("<unset>");
    }
    bool modified = value != defaultValue;
    switch (confOption.type)
    {
        case Type::plain:
            addValueMembers(document, key, value, defaultValue, confOption);
            break;
        case Type::file:
            addFileMembers(document, key, value, defaultValue, confOption);
            break;
        case Type::autoPem:
            addPemMembers(document, key, value, defaultValue, confOption);
            break;
    }
    addCommonMembers(document, confOption, key, modified);

}


void ConfigurationFormatter::addCommonMembers(rapidjson::Document& document,
                                               const KeyData& confOption, const std::string& key, bool modified)
{
    {
        const std::string descriptionPath = key + "/description";
        auto descPointer = rapidjson::Pointer(rapidjson::StringRef(descriptionPath.data(), descriptionPath.size()));
        descPointer.Set(document,
                        rapidjson::Value(confOption.description.begin(),
                                         gsl::narrow<rapidjson::SizeType>(confOption.description.size()),
                                         document.GetAllocator()),
                        document.GetAllocator());
    }
    {
        const std::string modifiedPath = key + "/isModified";
        auto modifiedPointer = rapidjson::Pointer(rapidjson::StringRef(modifiedPath.data(), modifiedPath.size()));
        modifiedPointer.Set(document, rapidjson::Value(modified), document.GetAllocator());
    }
    {
        bool deprecated = confOption.hasFlags(ConfigurationKeyFlags::deprecated);
        const std::string deprecatedPath = key + "/isDeprecated";
        auto deprecatedPointer = rapidjson::Pointer(rapidjson::StringRef(deprecatedPath.data(), deprecatedPath.size()));
        deprecatedPointer.Set(document, rapidjson::Value(deprecated), document.GetAllocator());
    }
}

std::string ConfigurationFormatter::getCategoryPath(ConfigurationKeyFlags flags)
{
    if ((flags & Flags::categoryEnvironment) != Flags{})
    {
        return "environment/";
    }
    if ((flags & Flags::categoryFunctional) != Flags{})
    {
        return "functional/";
    }
    if ((flags & Flags::categoryFunctionalStatic) != Flags{})
    {
        return "functionalStatic/";
    }
    if ((flags & Flags::categoryDebug) != Flags{})
    {
        return "debug/";
    }
    return {};
}

std::string ConfigurationFormatter::baseJsonPath(const KeyData& confOption)
{
    std::string category = getCategoryPath(confOption.flags);
    const auto keyEnvVar = std::string{confOption.environmentVariable};
    return std::string{"/"}.append(category).append(keyEnvVar);
}

void ConfigurationFormatter::appendFhirPackagesConfiguration(rapidjson::Document& document,
                                                             const fhirtools::FhirResourceViewConfiguration& config)
{
    const std::string rootPath = "/fhirPackagesDates";
    auto rootValuePtr = rapidjson::Pointer(rapidjson::StringRef(rootPath.data(), rootPath.size()));
    auto& rootValues = rootValuePtr.Create(document).SetArray();

    const auto& keyConfigs = config.kbvSchluesseltabellenConfiguration();
    const auto keyConfigsSubset = keyConfigs.entriesWithin({}, {});
    std::set<std::string> allKeys;
    for (const auto& entry : keyConfigsSubset)
    {
        allKeys.emplace(entry.id);
    }

    for (const auto& conf : config.allViews())
    {
        const auto startStr = conf->mStart.has_value() ? date::format("%Y-%m-%d", conf->mStart.value()) : "";
        const auto endStr = conf->mEnd.has_value() ? date::format("%Y-%m-%d", conf->mEnd.value()) : "";

        // Strip any date appendixes from the key name so that they remain fixed in the generated output.
        std::string k = conf->mId;
        for (const auto& entry : allKeys)
        {
            const std::string pat = "_" + entry;
            const auto pos = k.rfind(pat);
            if (pos != std::string::npos)
            {
                k.erase(pos, pat.length());
                break;
            }
        }

        auto& alloc = document.GetAllocator();

        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("name", k, alloc);
        obj.AddMember("from", startStr, alloc);
        obj.AddMember("until", endStr, alloc);
        std::string groups{};
        for (const auto& groupName : conf->mGroups)
        {
            groups += groupName + ";";
        }
        obj.AddMember("groups", groups, alloc);

        rootValues.PushBack(rapidjson::Value(obj, alloc), alloc);
    }
}
void ConfigurationFormatter::addValueMembers(rapidjson::Document& document, const std::string& key,
                                             const std::string& value, const std::string& defaultValue,
                                             const KeyData& confOption)
{
    static constexpr char redacted[] = "<redacted>";
    const std::string valuePath = key + "/value";
    auto valuePtr = rapidjson::Pointer(rapidjson::StringRef(valuePath.data(), valuePath.size()));
    const std::string defaultPath = key + "/default";
    auto defaultPtr = rapidjson::Pointer(rapidjson::StringRef(defaultPath.data(), defaultPath.size()));
    if (confOption.hasFlags(Flags::credential))
    {
        valuePtr.Set(document, redacted);
        defaultPtr.Set(document, redacted);
        // rapidjson::Pointer sha256Ptr{key + "/sha256"};
        // sha256Ptr.Set(document, Sha256::fromBin(value));
        // rapidjson::Pointer defaultSha256Ptr{key + "/defaultSha256"};
        // defaultSha256Ptr.Set(document, Sha256::fromBin(defaultValue));
    }
    else
    {
        valuePtr.Set(document, rapidjson::Value(value, document.GetAllocator()), document.GetAllocator());
        defaultPtr.Set(document, defaultValue);
    }
}

void ConfigurationFormatter::addFileMembers(rapidjson::Document& document, const std::string& key,
                                            const std::string& value, const std::string& defaultValue,
                                            const KeyData& confOption)
{
    static constexpr auto k10KiB = static_cast<uintmax_t>(1024 * 10);
    rapidjson::Pointer valuePtr{key + "/value"};
    const rapidjson::Pointer errorPtr{key + "/error"};

    const bool isCredential = confOption.hasFlags(Flags::credential);
    if (!isCredential)
    {
        valuePtr.Set(document, value);
    }
    std::filesystem::path path = value.starts_with(filePrefix)?value.substr(filePrefix.size()):value;

    if (path.empty())
    {
        errorPtr.Set(document, "value is empty");
        return;
    }

    std::error_code ec{};
    std::filesystem::directory_entry entry{path, ec};
    if (ec)
    {
        errorPtr.Set(document, ec.message());
        return;
    }
    if (! entry.is_regular_file())
    {
        errorPtr.Set(document, "not a regular file");
        return;
    }
    if (entry.file_size() > k10KiB)
    {
        errorPtr.Set(document, "file too large (>10kiB)");
        return;
    }
    std::ifstream file{path, std::ios_base::binary};
    file.unsetf(std::ios_base::skipws);
    if (! file.good())
    {
        errorPtr.Set(document, "cannot read file.");
        return;
    }
    std::string content;
    content.reserve(entry.file_size());
    std::istream_iterator<char> it{file};
    std::copy(it, std::istream_iterator<char>{}, std::back_inserter(content));
    if (file.bad())
    {
        errorPtr.Set(document, "read error.");
        return;
    }
    if (isCredential)
    {
        // rapidjson::Pointer sha256Ptr{key + "/sha256"};
        // sha256Ptr.Set(document, Sha256::fromBin(content));
        // rapidjson::Pointer defaultSha256Ptr{key + "/defaultSha256"};
        // defaultSha256Ptr.Set(document, Sha256::fromBin(defaultValue));

        // safely read data from file so we can be sure the configured value
        // is actually a filename not accidentally the literal credential
        valuePtr.Set(document, value);
        return;
    }
    const rapidjson::Pointer defaultPath{key + "/default"};
    defaultPath.Set(document, defaultValue);

    const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    const char* contentEnd = std::to_address(content.end());
    if (hasPrivateKeyHeader(content))
    {
        errorPtr.Set(document, "content contains PRIVATE KEY marker.");
        return;
    }
    const bool isBinary = (ctype.scan_not(std::ctype_base::print|std::ctype_base::space, content.data(), contentEnd) != contentEnd);
    if (isBinary)
    {
        rapidjson::Pointer base64Ptr{key + "/base64"};
        base64Ptr.Set(document, Base64::encode(content));
        return;
    }
    rapidjson::Pointer contentPtr{key + "/content"};
    contentPtr.Set(document, content);
}

void ConfigurationFormatter::addPemMembers(rapidjson::Document& document, const std::string& key,
                                           const std::string& value, const std::string& defaultValue,
                                           const KeyData& confOption)
{
    if (value.starts_with(filePrefix))
    {
        addFileMembers(document, key, value, defaultValue, confOption);
        return;
    }
    if (!value.starts_with(pemPrefix))
    {
        const rapidjson::Pointer errorPtr{key + "/error"};
        errorPtr.Set(document, "Value must be prefixed with either pem: or file://");
    }
    addValueMembers(document, key, value, defaultValue, confOption);
}
