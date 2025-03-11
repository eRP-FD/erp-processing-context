/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_PARAMETER_HXX

#include "shared/model/Resource.hxx"
#include "shared/model/ResourceNames.hxx"

#include <rapidjson/pointer.h>
#include <optional>
#include <string_view>
#include <tuple>


namespace model
{
class ParameterBase
{
public:
    static const rapidjson::Pointer resourceTypePointer;
    static const rapidjson::Pointer parameterArrayPointer;
    static const rapidjson::Pointer namePointer;
    static const rapidjson::Pointer resourcePointer;
    static const rapidjson::Pointer partArrayPointer;
    static const rapidjson::Pointer partValueCodePointer;
    static const rapidjson::Pointer partValueStringPointer;
    static const rapidjson::Pointer partValueBooleanPointer;
    static const rapidjson::Pointer workFlowTypePointer;
    static const rapidjson::Pointer workFlowSystemPointer;
    static const rapidjson::Pointer valueIdentifierSystemPointer;
    static const rapidjson::Pointer valueIdentifierValuePointer;
    static const rapidjson::Pointer valueDatePtr;

    static const rapidjson::Value* findPart(const rapidjson::Value& parameter, std::string_view name);
    static std::vector<const rapidjson::Value*> findParts(const rapidjson::Value& parameter, std::string_view name);
    static const rapidjson::Value* getResource(const rapidjson::Value& part);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
template<typename ParametersT>
class Parameters : public Resource<ParametersT>, public ParameterBase
{
public:
    static constexpr auto resourceTypeName = "Parameters";

    size_t count() const;
    const rapidjson::Value* findResourceParameter(std::string_view name) const;
    bool hasParameter(std::string_view name) const;
    const rapidjson::Value* findParameter(std::string_view name) const;
    rapidjson::Value& findOrAddParameter(std::string_view name);
    rapidjson::Value& findOrAddPart(rapidjson::Value& parameter, std::string_view name);
    rapidjson::Value& addPart(rapidjson::Value& parameter, std::string_view name);


    std::optional<std::string_view> getWorkflowSystem() const;

    void setValueIdentifier(rapidjson::Value& part, std::string_view system, std::string_view value);
    void setValueDate(rapidjson::Value& part, std::string_view date);
    void setResource(rapidjson::Value& part, const rapidjson::Value& resource);

    std::string_view getValueIdentifier(const rapidjson::Value& part) const;
    std::string_view getValueDate(const rapidjson::Value& part) const;
    NumberAsStringParserDocument getResourceDoc(const rapidjson::Value& part) const;

    void setId(std::string_view id);

private:
    rapidjson::Value& findOrAddInArrayWithName(rapidjson::Value& array, std::string_view name);

    friend class Resource<ParametersT>;
    using Resource<ParametersT>::Resource;
};

template<typename ParametersT>
size_t Parameters<ParametersT>::count() const
{
    return ResourceBase::valueSize(parameterArrayPointer);
}

template<typename ParametersT>
const rapidjson::Value* Parameters<ParametersT>::findResourceParameter(std::string_view name) const
{
    const auto result = ResourceBase::findMemberInArray(parameterArrayPointer, namePointer, name, resourcePointer);
    if (! result)
    {
        return nullptr;
    }
    return std::get<0>(result.value());
}

template<typename ParametersT>
bool Parameters<ParametersT>::hasParameter(std::string_view name) const
{
    const auto* result = ResourceBase::findMemberInArray(parameterArrayPointer, namePointer, name);
    return result != nullptr;
}

template<typename ParametersT>
std::optional<std::string_view> Parameters<ParametersT>::getWorkflowSystem() const
{
    return ResourceBase::getOptionalStringValue(workFlowSystemPointer);
}

template<typename ParametersT>
const rapidjson::Value* Parameters<ParametersT>::findParameter(std::string_view name) const
{
    return ResourceBase::findMemberInArray(parameterArrayPointer, namePointer, name);
}

template<typename ParametersT>
rapidjson::Value& Parameters<ParametersT>::findOrAddParameter(std::string_view name)
{
    auto* parameterArray = ResourceBase::getValue(parameterArrayPointer);
    if (! parameterArray)
    {
        parameterArray = &ResourceBase::createArray(parameterArrayPointer);
    }
    return findOrAddInArrayWithName(*parameterArray, name);
}

template<typename ParametersT>
rapidjson::Value& Parameters<ParametersT>::findOrAddPart(rapidjson::Value& parameter, std::string_view name)
{
    auto* partArray = partArrayPointer.Get(parameter);
    if (! partArray)
    {
        partArray = &ResourceBase::createArray(parameter, partArrayPointer);
    }
    return findOrAddInArrayWithName(*partArray, name);
}

template<typename ParametersT>
rapidjson::Value& Parameters<ParametersT>::addPart(rapidjson::Value& parameter, std::string_view name)
{
    auto* partArray = partArrayPointer.Get(parameter);
    if (! partArray)
    {
        partArray = &ResourceBase::createArray(parameter, partArrayPointer);
    }
    rapidjson::Value newParameter = NumberAsStringParserDocument::createEmptyObject();
    ResourceBase::setKeyValue(newParameter, namePointer, name);
    const auto idx = ResourceBase::addToArray(*partArray, std::move(newParameter));
    return (*partArray)[static_cast<rapidjson::SizeType>(idx)];
}


template<typename ParametersT>
rapidjson::Value& Parameters<ParametersT>::findOrAddInArrayWithName(rapidjson::Value& array, std::string_view name)
{
    auto* result = NumberAsStringParserDocument::findMemberInArray(&array, namePointer, name);
    if (! result)
    {
        rapidjson::Value newParameter = NumberAsStringParserDocument::createEmptyObject();
        ResourceBase::setKeyValue(newParameter, namePointer, name);
        ResourceBase::addToArray(array, std::move(newParameter));
        result = NumberAsStringParserDocument::findMemberInArray(&array, namePointer, name);
    }
    Expect(result, "Failed to add " + std::string(name) + " to parameters array");
    return *result;
}

template<typename ParametersT>
void Parameters<ParametersT>::setValueIdentifier(rapidjson::Value& part, std::string_view system,
                                                 std::string_view value)
{
    ResourceBase::setKeyValue(part, valueIdentifierSystemPointer, system);
    ResourceBase::setKeyValue(part, valueIdentifierValuePointer, value);
}

template<typename ParametersT>
void Parameters<ParametersT>::setValueDate(rapidjson::Value& part, std::string_view date)
{
    ResourceBase::setKeyValue(part, valueDatePtr, date);
}

template<typename ParametersT>
void Parameters<ParametersT>::setResource(rapidjson::Value& part, const rapidjson::Value& resource)
{
    ResourceBase::setKeyValue(part, resourcePointer, resource);
}

template<typename ParametersT>
std::string_view Parameters<ParametersT>::getValueIdentifier(const rapidjson::Value& part) const
{
    return ResourceBase::getStringValue(part, valueIdentifierValuePointer);
}

template<typename ParametersT>
std::string_view Parameters<ParametersT>::getValueDate(const rapidjson::Value& part) const
{
    return ResourceBase::getStringValue(part, valueDatePtr);
}

template<typename ParametersT>
NumberAsStringParserDocument Parameters<ParametersT>::getResourceDoc(const rapidjson::Value& part) const
{
    const auto* resource = getResource(part);
    ModelExpect(resource, "Resource not found in parameter part");
    NumberAsStringParserDocument doc;
    doc.CopyFrom(*resource, doc.GetAllocator());
    return doc;
}

template<typename ParametersT>
void Parameters<ParametersT>::setId(std::string_view id)
{
    static const rapidjson::Pointer idPtr{resource::ElementName::path(resource::elements::id)};
    Resource<ParametersT>::setValue(idPtr, id);
}

}

#endif//ERP_PROCESSING_CONTEXT_PARAMETER_HXX
