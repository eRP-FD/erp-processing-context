/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/ConfigurationFormatter.hxx"
#include "Configuration.hxx"
#include "RuntimeConfiguration.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>
#include <unordered_set>

std::string ConfigurationFormatter::formatAsJson(const Configuration& config,
                                                 const RuntimeConfigurationGetter& runtimeConfig, int flags)
{
    OpsConfigKeyNames confNames;
    rapidjson::Document document;
    // to determine unused "ERP_" variables, collect all with the prefix
    // and on each configuration value, we will remove it from this list once
    // we have seen it
    std::unordered_set<std::string> erpEnvVariables;
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
    for (const auto& confKey : confNames.allKeys())
    {
        const auto confOption = confNames.strings(confKey);
        std::string value;
        std::string defaultValue;
        if ((confOption.flags & flags) == 0)
        {
            continue;
        }

        if (confOption.flags & KeyData::array)
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
        bool deprecated = confOption.flags & KeyData::deprecated;

        if (confOption.flags & KeyData::credential)
        {
            value = "<redacted>";
            defaultValue = value;
        }
        std::string category = getCategoryPath(confOption.flags);

        const auto keyEnvVar = std::string{confOption.environmentVariable};
        const std::string key = std::string{"/"}.append(category).append(keyEnvVar);
        {
            const std::string valuePath = key + "/value";
            auto p = rapidjson::Pointer(rapidjson::StringRef(valuePath.data(), valuePath.size()));
            p.Set(document, rapidjson::Value(value, document.GetAllocator()), document.GetAllocator());
        }
        {
            const std::string descriptionPath = key + "/description";
            auto descPointer = rapidjson::Pointer(rapidjson::StringRef(descriptionPath.data(), descriptionPath.size()));
            descPointer.Set(document, rapidjson::Value(confOption.description.data(), document.GetAllocator()),
                            document.GetAllocator());
        }
        {
            const std::string defaultPath = key + "/default";
            auto defaultPointer = rapidjson::Pointer(rapidjson::StringRef(defaultPath.data(), defaultPath.size()));
            defaultPointer.Set(document, rapidjson::Value(defaultValue.data(), document.GetAllocator()),
                               document.GetAllocator());
        }
        {
            const std::string modifiedPath = key + "/isModified";
            auto modifiedPointer = rapidjson::Pointer(rapidjson::StringRef(modifiedPath.data(), modifiedPath.size()));
            modifiedPointer.Set(document, rapidjson::Value(modified), document.GetAllocator());
        }
        {
            const std::string deprecatedPath = key + "/isDeprecated";
            auto deprecatedPointer =
                rapidjson::Pointer(rapidjson::StringRef(deprecatedPath.data(), deprecatedPath.size()));
            deprecatedPointer.Set(document, rapidjson::Value(deprecated), document.GetAllocator());
        }

        if (erpEnvVariables.contains(keyEnvVar))
        {
            erpEnvVariables.erase(keyEnvVar);
        }
    }
    const rapidjson::Pointer unusedVarsPointer("/unusedVariables");
    auto& unusedVarsArray = unusedVarsPointer.Create(document).SetArray();
    for (const auto& envVar : erpEnvVariables)
    {
        unusedVarsArray.PushBack(rapidjson::Value(envVar, document.GetAllocator()), document.GetAllocator());
    }
    if ((KeyData::ConfigurationKeyFlags::categoryRuntime & flags) != 0)
    {
        appendRuntimeConfiguration(document, runtimeConfig);
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    document.Accept(writer);
    return buffer.GetString();
}

std::string ConfigurationFormatter::getCategoryPath(int flags)
{
    if (flags & KeyData::categoryEnvironment)
    {
        return "environment/";
    }
    if (flags & KeyData::categoryFunctional)
    {
        return "functional/";
    }
    if (flags & KeyData::categoryFunctionalStatic)
    {
        return "functionalStatic/";
    }
    if (flags & KeyData::categoryDebug)
    {
        return "debug/";
    }
    return {};
}

void ConfigurationFormatter::appendRuntimeConfiguration(rapidjson::Document& document,
                                                        const RuntimeConfigurationGetter& runtimeConfig)
{
    const rapidjson::Pointer acceptPN3Pointer("/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3} +
                                              "/value");
    acceptPN3Pointer.Set(
        document, rapidjson::Value((runtimeConfig.isAcceptPN3Enabled() ? "true" : "false"), document.GetAllocator()),
        document.GetAllocator());
    if (runtimeConfig.isAcceptPN3Enabled())
    {
        const rapidjson::Pointer acceptPN3ExpiryPointer(
            "/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3_expiry} + "/value");
        acceptPN3ExpiryPointer.Set(
            document, rapidjson::Value(runtimeConfig.getAcceptPN3Expiry().toXsDateTime(), document.GetAllocator()),
            document.GetAllocator());
    }
    const rapidjson::Pointer acceptPN3MaxActive(
        "/runtime/" + std::string{RuntimeConfiguration::parameter_accept_pn3_max_active} + "/value");

    // std::chrono::hh_mm_ss::operator<< not yet implemented in gcc-12.
    const std::chrono::hh_mm_ss hhmmss(RuntimeConfiguration::accept_pn3_max_active);
    acceptPN3MaxActive.Set(
        document,
        rapidjson::Value(
            (std::ostringstream{} << hhmmss.hours() << " " << hhmmss.minutes() << " " << hhmmss.seconds()).str(),
            document.GetAllocator()),
        document.GetAllocator());
}
