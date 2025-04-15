/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Coding.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/ResourceNames.hxx"

#include <rapidjson/pointer.h>

namespace
{
using model::resource::ElementName;
const rapidjson::Pointer systemPtr{ElementName::path(model::resource::elements::system)};
const rapidjson::Pointer versionPtr{ElementName::path(model::resource::elements::version)};
const rapidjson::Pointer codePtr{ElementName::path(model::resource::elements::code)};
const rapidjson::Pointer displayPtr{ElementName::path(model::resource::elements::display)};
const rapidjson::Pointer userSelectedPtr{ElementName::path(model::resource::ElementName{"userSelected"})};
}

namespace model
{

Coding::Coding(const rapidjson::Value& object)
{
    mSystem = NumberAsStringParserDocument::getOptionalStringValue(object, systemPtr);
    mVersion = NumberAsStringParserDocument::getOptionalStringValue(object, versionPtr);
    mCode = NumberAsStringParserDocument::getOptionalStringValue(object, codePtr);
    mDisplay = NumberAsStringParserDocument::getOptionalStringValue(object, displayPtr);
    const auto* userSelected = userSelectedPtr.Get(object);
    if (userSelected && userSelected->IsBool())
    {
        mUserSelected = userSelected->GetBool();
    }
}

Coding::Coding(const std::string_view& system, const std::string_view& code)
    : mSystem(system)
    , mCode(code)
{
}

void Coding::setSystem(const std::string& system)
{
    mSystem = system;
}
void Coding::setVersion(const std::string& version)
{
    mVersion = version;
}
void Coding::setCode(const std::string& code)
{
    mCode = code;
}
void Coding::setDisplay(const std::string& display)
{
    mDisplay = display;
}
void Coding::setUserSelected(bool userSelected)
{
    mUserSelected = userSelected;
}

const std::optional<std::string>& Coding::getSystem() const
{
    return mSystem;
}
const std::optional<std::string>& Coding::getVersion() const
{
    return mVersion;
}
const std::optional<std::string>& Coding::getCode() const
{
    return mCode;
}
const std::optional<std::string>& Coding::getDisplay() const
{
    return mDisplay;
}
const std::optional<bool>& Coding::getUserSelected() const
{
    return mUserSelected;
}
}
