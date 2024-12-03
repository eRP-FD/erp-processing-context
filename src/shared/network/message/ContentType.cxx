/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/message/ContentType.hxx"

#include "shared/util/String.hxx"

#include "shared/util/TLog.hxx"

namespace {
    const std::string emptyParameter;
}


ContentType ContentType::fromString (const std::string& contentTypeValue)
{
    std::string type;
    std::unordered_map<std::string, std::string> parameters;
    if ( ! contentTypeValue.empty())
    {
        const auto parts = String::splitWithStringQuoting(contentTypeValue, ';');
        bool isFirstPart = true;

        for (const auto& part : parts)
        {
            if (isFirstPart)
            {
                type = String::toLower(part);
                isFirstPart = false;
            }
            else
            {
                // Optional secondary parts after one ore more semicolons are attributes in the form key=value.
                const auto firstEqual = part.find('=');
                if (firstEqual == std::string::npos)
                    TLOG(WARNING) << "ignoring attribute of Content-Type with unsupported format: '" << part << "'";
                else
                {
                    std::string key = String::toLower(String::trim(part.substr(0, firstEqual)));
                    std::string value = String::trim(String::removeEnclosing("\"", "\"", String::trim(part.substr(firstEqual+1))));
                    parameters.emplace(std::move(key), std::move(value));
                }
            }
        }
    }

    return ContentType(std::move(type), std::move(parameters));
}


ContentType::ContentType (
    std::string type,
    std::unordered_map<std::string,std::string> parameters)
    : mType(std::move(type)),
      mParameters(std::move(parameters))
{
}


const std::string& ContentType::getType (void) const
{
    return mType;
}


bool ContentType::hasParameter (const std::string& key) const
{
    return mParameters.find(key) != mParameters.end();
}


const std::string& ContentType::getParameterValue (const std::string& key) const
{
    const auto candidate = mParameters.find(key);
    if (candidate == mParameters.end())
        return emptyParameter;
    else
        return candidate->second;
}
