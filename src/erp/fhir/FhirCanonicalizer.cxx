/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/FhirCanonicalizer.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/pointer.h>
#include <regex>
#include <set>

using namespace std::string_literals;
using namespace model;
using fhirtools::FhirElement;
using fhirtools::FhirStructureDefinition;

namespace rj = rapidjson;

namespace {
    class ImmersionDepthGuard
    {
    public:
        ImmersionDepthGuard(size_t& immersionDepth) :
            mImmersionDepth(&immersionDepth)
        {
            (*mImmersionDepth)++;
        }
        ~ImmersionDepthGuard()
        {
            (*mImmersionDepth)--;
        }
    private:
        size_t* mImmersionDepth;
    };
}

std::string FhirCanonicalizer::serialize(const rj::Value& resource)
{
    std::ostringstream buffer;
    size_t immersionDepth = 0;
    serializeResource(immersionDepth, buffer, "", resource);
    return buffer.str();
}

void FhirCanonicalizer::serializeResource( // NOLINT(misc-no-recursion)
    size_t& immersionDepth,
    std::ostringstream& buffer,
    const std::string& name,
    const rj::Value& object)
{
    static const rapidjson::Pointer resourceTypePointer{ resource::ElementName::path(resource::elements::resourceType) };
    const rj::Value* resourceTypeValue = resourceTypePointer.Get(object);
    ModelExpect(resourceTypeValue != nullptr, "resourceType not set.");

    std::string resourceTypeId{NumberAsStringParserDocument::getStringValueFromValue(resourceTypeValue)};
    const FhirStructureDefinition* objectStructDef = Fhir::instance().backend().findTypeById(resourceTypeId);
    ModelExpect(objectStructDef != nullptr, "Unknown resource type: "s.append(resourceTypeId));

    serializeObject(immersionDepth, buffer, nullptr, nullptr, *objectStructDef, name, object);
}

void FhirCanonicalizer::serializeObject( // NOLINT(misc-no-recursion)
    size_t& immersionDepth,
    std::ostringstream& buffer,
    const FhirStructureDefinition* backboneStructDef,
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& objectStructDef,
    const std::string& objectName,
    const rj::Value& object)
{
    ImmersionDepthGuard immersionDepthGuard(immersionDepth);

    if (objectName.empty())
    {
        buffer << "{";
    }
    else
    {
        buffer << "\"" << objectName << "\":{";
    }

    std::map<std::string, const rj::Value*> sortedMembers =
        getSortedMembers(immersionDepth, backboneStructDef, backboneElement, objectStructDef, object);

    size_t idx = 0;
    for (const auto& itr : sortedMembers)
    {
        if (itr.first == std::string(resource::elements::resourceType))
        {
            // "resourceType" does not belong to the elements of the structure definition
            // but must be written to the output buffer. Please note that the value of the
            // resourceType must not contain white spaces at all.
            serializePrimitiveValue(buffer, objectStructDef, itr.first, *itr.second);
        }
        else
        {
            std::string elementPath = buildElementPath(backboneElement, objectStructDef, itr.first);
            auto element = backboneStructDef == nullptr ? objectStructDef.findElement(elementPath) : backboneStructDef->findElement(elementPath);
            ModelExpect(element != nullptr, "Element " + elementPath + " does not belong to FHIR structure definition " + objectStructDef.typeId());
            const FhirStructureDefinition* elementStructDef =
                Fhir::instance().backend().findTypeById(element->typeId());
            ModelExpect(elementStructDef != nullptr, "Element " + element->typeId() + " is not a valid resource type");

            const FhirStructureDefinition* elementBackboneStructDef = nullptr;
            std::shared_ptr<const FhirElement> elementBackboneElement = nullptr;

            if (element->isBackbone())
            {
                // For backbone elements we remain in the current structure definition but have to address "childs" of the backbone element.
                elementBackboneStructDef = &objectStructDef;
                elementBackboneElement = std::move(element);
            }

            serializeValue(immersionDepth, buffer, elementBackboneStructDef, elementBackboneElement, *elementStructDef, itr.first, *itr.second);
        }

        if (idx++ < sortedMembers.size() - 1)
        {
            buffer << ",";
        }
    }
    buffer << "}";
}

void FhirCanonicalizer::serializeArray( // NOLINT(misc-no-recursion)
    size_t& immersionDepth,
    std::ostringstream& buffer,
    const FhirStructureDefinition* backboneStructDef,
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& objectStructDef,
    const std::string& arrayName,
    const rj::Value& array)
{
    ImmersionDepthGuard immersionDepthGuard(immersionDepth);

    if (arrayName.empty())
    {
        buffer << "[";
    }
    else
    {
        buffer << "\"" << arrayName << "\":[";
    }

    for (rj::SizeType idx = 0; idx < array.Size(); ++idx)
    {
        serializeValue(immersionDepth, buffer, backboneStructDef, backboneElement, objectStructDef, "", array[idx]);

        if (idx < array.Size() - 1)
        {
            buffer << ",";
        }
    }
    buffer << "]";
}

void FhirCanonicalizer::serializePrimitiveValue(
    std::ostringstream& buffer,
    const FhirStructureDefinition& elementStructDef,
    const std::string& name,
    const rj::Value& value)
{
    if (!name.empty())
    {
        buffer << "\"" << name << "\":";
    }

    if (NumberAsStringParserDocument::valueIsNull(value))
    {
        buffer << "null";
    }
    else if (NumberAsStringParserDocument::valueIsFalse(value))
    {
        buffer << "false";
    }
    else if (NumberAsStringParserDocument::valueIsTrue(value))
    {
        buffer << "true";
    }
    else if (NumberAsStringParserDocument::valueIsString(value))
    {
        std::string valueTmp = std::string(NumberAsStringParserDocument::getStringValueFromValue(&value));
        if (doubleWhitespacesHaveToBeRemoved(elementStructDef))
        {
            removeDoubleWhitespaces(valueTmp);
        }
        buffer << "\"" << valueTmp << "\"";
    }
    else if (NumberAsStringParserDocument::valueIsNumber(value))
    {
        buffer << std::string(NumberAsStringParserDocument::getStringValueFromValue(&value));
    }
    else
    {
        ModelFail("Invalid Type of Json Object");
    }
}

void FhirCanonicalizer::serializeValue( // NOLINT(misc-no-recursion)
    size_t& immersionDepth,
    std::ostringstream& buffer,
    const FhirStructureDefinition* backboneStructDef,
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& elementStructDef,
    const std::string& name,
    const rapidjson::Value& value)
{
    if (value.IsObject())
    {
        if (elementStructDef.kind() == FhirStructureDefinition::Kind::resource)
        {
            serializeResource(immersionDepth, buffer, name, value);
        }
        else
        {
            serializeObject(immersionDepth, buffer, backboneStructDef, backboneElement, elementStructDef, name, value);
        }
    }
    else if (value.IsArray())
    {
        serializeArray(immersionDepth, buffer, backboneStructDef, backboneElement, elementStructDef, name, value);
    }
    else
    {
        serializePrimitiveValue(buffer, elementStructDef, name, value);
    }
}

std::map<std::string, const rj::Value*> FhirCanonicalizer::getSortedMembers(
    size_t immersionDepth,
    const FhirStructureDefinition* backboneStructDef,
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& objectStructDef,
    const rj::Value& object)
{
    std::map<std::string, const rj::Value*> sortedMembers;
    for (rj::Value::ConstMemberIterator itr = object.MemberBegin(); itr != object.MemberEnd(); ++itr)
    {
        std::string name = std::string(itr->name.GetString(), itr->name.GetStringLength());
        if (!elementHasToBeRemoved(immersionDepth, backboneStructDef, backboneElement, objectStructDef, name))
        {
            sortedMembers.insert(std::make_pair(name, &itr->value));
        }
    }
    return sortedMembers;
}

std::string FhirCanonicalizer::buildElementPath(
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& objectStructDef,
    const std::string& objectName)
{
    std::string name = String::starts_with(objectName, "_") ? &objectName[1] : objectName;
    if (backboneElement != nullptr)
    {
        return backboneElement->name() + "." + name;
    }
    return objectStructDef.typeId() + "." + name;
}

void FhirCanonicalizer::removeDoubleWhitespaces(std::string& value)
{
    std::regex expr(R"([\s]{2,})");
    value = std::regex_replace(value, expr, " ");
}

bool FhirCanonicalizer::elementHasToBeRemoved(
    size_t immersionDepth,
    const FhirStructureDefinition* backboneStructDef,
    const std::shared_ptr<const FhirElement>& backboneElement,
    const FhirStructureDefinition& objectStructDef,
    const std::string& objectName)
{
    // "resourceType" does not belong to the elements of the structure
    // definition but must be written to the output buffer.
    if (objectName == std::string(resource::elements::resourceType))
    {
        return false;
    }

    // For the root elements:
    // - The narrative (Resource.text) is omitted.
    // - In addition to narrative (Resource.text), the Resource.meta element is removed.
    // - Also the signature element has to be removed.
    A_19029_05.start("Resource.meta nur im 'root-Element' entfernen");
    if( immersionDepth == 1 && objectStructDef.kind() == FhirStructureDefinition::Kind::resource)
    {
        static const std::map<std::string, std::string>
            elementsToBeRemoved{{"meta", "Meta"}, {"text", "Narrative"}, {"signature", "Signature"}};
        const auto elemToBeRemoved = elementsToBeRemoved.find(objectName);
        if (elemToBeRemoved != elementsToBeRemoved.end())
        {
            std::string elementPath = buildElementPath(backboneElement, objectStructDef, objectName);
            auto element = backboneStructDef == nullptr ? objectStructDef.findElement(elementPath) : backboneStructDef->findElement(elementPath);
            ModelExpect(element != nullptr, "Element " + elementPath + " does not belong to FHIR structure definition " + objectStructDef.typeId());
            const FhirStructureDefinition* elementStructDef =
                Fhir::instance().backend().findTypeById(element->typeId());
            ModelExpect(elementStructDef != nullptr, "Element " + element->typeId() + " is not a valid resource type");
            if (elemToBeRemoved->second == elementStructDef->typeId())
            {
                return true;
            }
        }
    }
    A_19029_05.finish();
    return false;
}

bool FhirCanonicalizer::doubleWhitespacesHaveToBeRemoved(const FhirStructureDefinition& elementStructDef)
{
    // No whitespace other than single spaces in property values and in the xhtml in the Narrative.
    // For the following data types whitespaces must not be removed:
    static const std::set<std::string> dataTypesToKeepWhitespaces = { "id", "base64Binary", "uri" };

    return (dataTypesToKeepWhitespaces.find(elementStructDef.typeId()) == dataTypesToKeepWhitespaces.end());
}
