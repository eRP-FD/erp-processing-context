/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Parameters.hxx"
#include "shared/model/ResourceNames.hxx"

#include <libxml/xpath.h>
#include <magic_enum/magic_enum.hpp>
#include <tuple>


namespace model
{

using namespace resource;


const rapidjson::Pointer ParameterBase::resourceTypePointer{ElementName::path(elements::resourceType)};
const rapidjson::Pointer ParameterBase::parameterArrayPointer{ElementName::path(elements::parameter)};
const rapidjson::Pointer ParameterBase::namePointer{ElementName::path(elements::name)};
const rapidjson::Pointer ParameterBase::resourcePointer{ElementName::path(elements::resource)};
const rapidjson::Pointer ParameterBase::partArrayPointer{ElementName::path(elements::part)};
const rapidjson::Pointer ParameterBase::partValueCodePointer{ElementName::path(elements::valueCode)};
const rapidjson::Pointer ParameterBase::partValueStringPointer{ElementName::path(elements::valueString)};
const rapidjson::Pointer ParameterBase::partValueBooleanPointer{ElementName::path(elements::valueBoolean)};
const rapidjson::Pointer ParameterBase::workFlowTypePointer{
    ElementName::path(elements::parameter, 0, elements::valueCoding, elements::code)};
const rapidjson::Pointer ParameterBase::workFlowSystemPointer{
    ElementName::path(elements::parameter, 0, elements::valueCoding, elements::system)};
const rapidjson::Pointer ParameterBase::valueIdentifierSystemPointer{
    ElementName::path(elements::valueIdentifier, elements::system)};
const rapidjson::Pointer ParameterBase::valueIdentifierValuePointer{
    ElementName::path(elements::valueIdentifier, elements::value)};
const rapidjson::Pointer ParameterBase::valueDatePtr{ElementName::path(elements::valueDate)};


const rapidjson::Value* ParameterBase::findPart(const rapidjson::Value& parameter, std::string_view name)
{
    const auto* partArray = partArrayPointer.Get(parameter);
    return NumberAsStringParserDocument::findMemberInArray(partArray, namePointer, name);
}

std::vector<const rapidjson::Value*> ParameterBase::findParts(const rapidjson::Value& parameter, std::string_view name)
{
    std::vector<const rapidjson::Value*> parts;
    const auto* partArray = partArrayPointer.Get(parameter);
    if (partArray && partArray->IsArray())
    {
        for (const auto& entry : partArray->GetArray())
        {
            if (const auto entryName = NumberAsStringParserDocument::getOptionalStringValue(entry, namePointer))
            {
                if (entryName == name)
                {
                    parts.emplace_back(&entry);
                }
            }
        }
    }
    return parts;
}

const rapidjson::Value* ParameterBase::getResource(const rapidjson::Value& part)
{
    const auto* result = resourcePointer.Get(part);
    if (result == nullptr)
    {
        return nullptr;
    }
    return NumberAsStringParserDocument::valueIsObject(*result) ? result : nullptr;
}

}// namespace model
