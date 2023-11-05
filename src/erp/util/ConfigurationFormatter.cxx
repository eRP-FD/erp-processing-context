/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>

#include <unordered_set>

std::string ConfigurationFormatter::formatAsJson(const Configuration& config, int flags)
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
        std::string category;
        if (confOption.flags & KeyData::categoryEnvironment)
        {
            category = "environment/";
        }
        else if (confOption.flags & KeyData::categoryFunctional)
        {
            category = "functional/";
        }
        else if (confOption.flags & KeyData::categoryFunctionalStatic)
        {
            category = "functionalStatic/";
        }
        else if (confOption.flags & KeyData::categoryDebug)
        {
            category = "debug/";
        }
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
            descPointer.Set(document, rapidjson::Value(confOption.description.data(), document.GetAllocator()), document.GetAllocator());
        }
        {
            const std::string defaultPath = key + "/default";
            auto defaultPointer = rapidjson::Pointer(rapidjson::StringRef(defaultPath.data(), defaultPath.size()));
            defaultPointer.Set(document, rapidjson::Value(defaultValue.data(), document.GetAllocator()), document.GetAllocator());
        }
        {
            const std::string modifiedPath = key + "/isModified";
            auto modifiedPointer = rapidjson::Pointer(rapidjson::StringRef(modifiedPath.data(), modifiedPath.size()));
            modifiedPointer.Set(document, rapidjson::Value(modified), document.GetAllocator());
        }
        {
            const std::string deprecatedPath = key + "/isDeprecated";
            auto deprecatedPointer = rapidjson::Pointer(rapidjson::StringRef(deprecatedPath.data(), deprecatedPath.size()));
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
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    document.Accept(writer);
    return buffer.GetString();
}
