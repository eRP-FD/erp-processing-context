/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "FhirSAXHandler.hxx"

#include "erp/fhir/FhirStructureRepository.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/TLog.hxx"
#include "erp/xml/XmlMemory.hxx"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <libxml/xmlstring.h>
#include <utility>


using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
bool hasNonNullElement(const rapidjson::Value::ConstArray& array)
{
    return std::find_if(array.begin(), array.end(), [](const auto& val){ return !val.IsNull();}) != array.end();
}

bool hasValue(const rapidjson::Value& val)
{
    if (val.IsArray())
    {
        return hasNonNullElement(val.GetArray());
    }
    return !val.IsNull() && (!val.IsObject() || !val.ObjectEmpty());
}

}

/// @brief Parser Context
class FhirSaxHandler::Context {
public:
    template <typename valueArgT>
    explicit Context(const FhirStructureDefinition& inType,
                   const FhirElement& inBackBone,
                   std::string inName, valueArgT&& valueArg)
        : name(std::move(inName))
        , value(std::forward<valueArgT>(valueArg))
        , subObjectOfPrimitive{value.IsArray()?rapidjson::kArrayType:rapidjson::kNullType}
        , mType(&inType)
        , mElement(&inBackBone)
    {

    }
    explicit Context(const FhirStructureDefinition& inType,
                   const FhirElement& inBackBone,
                   std::string inName, rapidjson::Value&& inValue)
        : name(std::move(inName))
        , value(std::move(inValue))
        , subObjectOfPrimitive{value.IsArray()?rapidjson::kArrayType:rapidjson::kNullType}
        , mType(&inType)
        , mElement(&inBackBone)
    {

    }

    void setResourceType(const FhirStructureRepository& repo,
                         FhirSaxHandler& parent,
                        const std::string_view& resourceTypeName);

    const FhirStructureDefinition& type() const { return *mType; }
    const FhirElement& element() const { return *mElement; }

    std::string name;
    bool isSubObjectOfPrimitive = false;
    rapidjson::Value value;
    rapidjson::Value subObjectOfPrimitive{rapidjson::kNullType};
    /// points to the ArrayElement handled in this context; nullptr if this context doesn't represent an array
    const FhirElement* currentArray = nullptr;

    // depth represets the depth in the XML-Tree, that are accounted for this stack instance
    // it is incremented, when a start tag doesn't push something onto the stack
    // If for example depth equals 3, then the parser must hit 3 end tags until this element is removed
    size_t depth = 0;
private:
    const FhirStructureDefinition* mType = nullptr;
    const FhirElement* mElement = nullptr;
};

/// resources are polymorphic. This function is called, when the actual type is known
/// and changes mType and mElement apropriately. It also adds the `resourceType` json field.
void FhirSaxHandler::Context::setResourceType(const FhirStructureRepository& repo,
                                     FhirSaxHandler& parent,
                                     const std::string_view& resourceTypeName)
{
    Expect3(value.IsObject(), "top item must be Object.", std::logic_error);
    const auto* resourceType = repo.findTypeById(std::string{resourceTypeName});
    ErpExpect(resourceType != nullptr, HttpStatus::BadRequest, "Unknown resourceType: "s.append(resourceTypeName));
    ErpExpect(resourceType->isDerivedFrom(repo , mType->url()), HttpStatus::BadRequest,
                "Resource " + resourceType->url() + " is not derived from " + mType->url());
    value.AddMember("resourceType", parent.mResult.makeString(resourceTypeName), parent.mResult.GetAllocator());
    mType = resourceType;
    mElement = resourceType->findElement(resourceType->typeId());
    Expect3(mElement != nullptr, "missing root element: " + resourceType->url(), std::logic_error);
}


FhirSaxHandler::FhirSaxHandler(const FhirStructureRepository& repo)
    : mStructureRepo(repo)
    , mBackBoneType(mStructureRepo.findTypeById("BackboneElement"))
{
    Expect3(mBackBoneType != nullptr, "Type BackboneElement not defined.", std::logic_error);
}

FhirSaxHandler::~FhirSaxHandler() = default;

model::NumberAsStringParserDocument FhirSaxHandler::parseXMLintoJSON(const FhirStructureRepository& repo,
                                                     const std::string_view& xmlDocument,
                                                     XmlValidatorContext* schemaValidationContext)
{
    return FhirSaxHandler{repo}.parseXMLintoJSONInternal(xmlDocument, schemaValidationContext);
}

void FhirSaxHandler::validateXML(const FhirStructureRepository& repo, const std::string_view& xmlDocument,
                                 XmlValidatorContext& schemaValidationContext)
{
    FhirSaxHandler{repo}.validateStringView(xmlDocument, schemaValidationContext);
}

model::NumberAsStringParserDocument FhirSaxHandler::parseXMLintoJSONInternal(
    const std::string_view& xmlDocument,
    XmlValidatorContext* schemaValidationContext)
{
    if (schemaValidationContext)
        parseAndValidateStringView(xmlDocument, *schemaValidationContext);
    else
        parseStringView(xmlDocument);
    return std::move(mResult);
}

void FhirSaxHandler::endDocument()
{
    Expect3(mStack.empty() || mExceptionPtr, "Stack not empty at end of Document.", std::logic_error);
}

void FhirSaxHandler::startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int nbNamespaces,
                                  const xmlChar ** namespaces, int nbDefaulted,
                                  const SaxHandler::AttributeList& attributes)
{
    (void)prefix;
    (void)nbDefaulted;
    XmlStringView localnameView{localname};
    if (uri != Fhir::namespaceUri)
    {
        ErpExpect(!mStack.empty(), HttpStatus::BadRequest, "Not a FHIR Document.");
        if (uri == xhtmlNamespaceUri)
        {
            startXHTMLElement(localnameView, nbNamespaces, namespaces, attributes);
            return;
        }
        ErpFail(HttpStatus::BadRequest,
                  "Unsupported namespace in xml document: "s.append(reinterpret_cast<const char*>(uri)));
    }
    startFHIRElement(localnameView, attributes);
}

void FhirSaxHandler::startFHIRElement(const XmlStringView& localname, const AttributeList& attributes)
{
    using namespace xmlHelperLiterals;
    TVLOG(3) << "startElement: " << std::string_view{localname};
    if (mStack.empty())
    {
        pushRootResource(localname);
        return;
    }
    auto& parentItem = mStack.back();
    const auto& parentElement = parentItem.element();
    TVLOG(3) << "  handle subelement of: " << parentElement.name() << ":" << parentElement.typeId() << (parentElement.isArray()?"[]":"");
    const auto* const parentElementType = mStructureRepo.findTypeById(parentElement.typeId());
    Expect3(parentElementType != nullptr, "Failed to get Backbone Type: " + parentElement.name(), std::logic_error);
    ErpExpect(!parentElementType->isSystemType(), HttpStatus::BadRequest, "System types should not have sub-elements.");
    if (parentItem.currentArray == nullptr &&
        parentElementType->kind() == FhirStructureDefinition::Kind::primitiveType)
    {
        pushContextForSubObjectOfPrimitive(localname);
    }
    if (parentElementType->kind() == FhirStructureDefinition::Kind::resource && !parentElement.isRoot())
    {
        parentItem.setResourceType(mStructureRepo, *this, localname);
        ++parentItem.depth;
        return;
    }
    std::string elementId;
    const auto& [elementType, element] = getTypeAndElement(*parentElementType, &parentItem.type(), parentElement, localname);
    TVLOG(3) << "Element: " << element.name() << ":" << elementType.typeId() << (element.isArray()?"[]":"");
    if (parentItem.currentArray != &element) // no need to check for .currentArray == nullptr, because &element != nullptr
    {
        pushArrayContext(parentItem, parentElement, element, localname);
    }
    Expect3(&elementType != mBackBoneType, "elementType should not be Backbone", std::logic_error);
    pushJsonField(localname, attributes, elementType, element);
    if (elementType.kind() == FhirStructureDefinition::Kind::primitiveType)
    {
        pushObject(localname, attributes, elementType, element);
        joinTopStackElements();
    }
}

void FhirSaxHandler::pushArrayContext(const Context& parentItem, const FhirElement& parentElement,
                                   const FhirElement& element, const XmlStringView& localname)
{

    // arrays are special case they are implemented by using two contexts
    // one context holding the actual array and one context for each element.
    if (parentItem.currentArray)
    {
        // An array in the XML-Structure is represented by a sequence of identical elements.
        // They are NOT embedded in a separate start/end tag, therfore we can detect
        // the end of the array when the tag name changes.
        TVLOG(3) << "  element change";
        joinTopStackElements();
    }
    if (element.isArray())
    {
        // only when entering a new array an array-context is created.
        auto& arrayItem = mStack.emplace_back(mStack.back().type(), parentElement, std::string{localname},
                                              rapidjson::kArrayType);
        arrayItem.currentArray = &element;
    }
}

void FhirSaxHandler::pushJsonField(const std::string_view& name, const SaxHandler::AttributeList& attributes, const FhirStructureDefinition& type, const FhirElement& element)
{
    auto kind = type.kind();
    auto value = attributes.findAttribute("value");
    if (kind == FhirStructureDefinition::Kind::primitiveType)
    {
        if (!value)
        {
            mStack.emplace_back(type, element, std::string{name}, rapidjson::kNullType);
            return;
        }
        kind = systemTypeFor(type).kind();
    }
    switch (kind)
    {
        using K = FhirStructureDefinition::Kind;
        case K::systemBoolean:
        {
            ErpExpect(value.has_value(), HttpStatus::BadRequest, "Missing value for system type: " + getPath());
            pushBoolean(name, value->value(), type, element);
            return;
        }
        case K::systemDouble:
        case K::systemInteger:
        {
            ErpExpect(value.has_value(), HttpStatus::BadRequest, "Missing value for system type: " + getPath());
            pushNumber(name, value->value(), type, element);
            return;
        }
        case K::systemString:
        {
            ErpExpect(value.has_value(), HttpStatus::BadRequest, "Missing value for system type: " + getPath());
            pushString(name, value->value(), type, element);
            return;
        }
        case K::complexType:
        case K::resource:
        {
            pushObject(name, attributes, type, element);
            return;
        }
        case K::logical:
            Fail("Unsupported structure kind: " + to_string(type.kind()));
        case K::primitiveType:
            Fail("Primitive type should have been resolved to system type.");
    }
    Fail("Invalid value for Kind: " + to_string(kind));
}

void FhirSaxHandler::pushObject(const std::string_view& name, const AttributeList& attributes,
                         const FhirStructureDefinition& type,
                         const FhirElement& element)
{
    auto& newObject = mStack.emplace_back(type, element, std::string{name}, rapidjson::kObjectType);
    bool isSubObjectOfPrimitive = type.kind() == FhirStructureDefinition::Kind::primitiveType;
    newObject.isSubObjectOfPrimitive = isSubObjectOfPrimitive;

    for (size_t i = 0 ; i < attributes.size(); ++i)
    {
        auto attribute = attributes.get(i);
        auto uri = attribute.uri();
        if (!uri.has_value() || uri.value() == Fhir::namespaceUri)
        {
            if (attribute.localname() == "value" && isSubObjectOfPrimitive)
            {
                continue;
            }
            std::string elementId = makeElementId(element, attribute.localname());
            const auto [fieldType, fieldElement] = getTypeAndElement(type, nullptr, element, attribute.localname());
            ErpExpect(fieldElement.representation() == FhirElement::Representation::xmlAttr, HttpStatus::BadRequest,
                      "Field on " + getPath() + " may not be represented as attribute: "s.append(attribute.localname()));
            ErpExpect(fieldType.kind() == FhirStructureDefinition::Kind::primitiveType || fieldType.isSystemType(), HttpStatus::BadRequest,
                      "Non-primitive type for attibute " + getPath().append(attribute.localname()) + ": "s.append(fieldElement.typeId()));
            auto systemType = systemTypeFor(fieldType);
            switch (systemType.kind())
            {
                using K = FhirStructureDefinition::Kind;
                case K::primitiveType:
                case K::complexType:
                case K::resource:
                case K::logical:
                    Fail("Unexpected non-system Type on primitive type: " + fieldType.url());
                case K::systemBoolean:
                    pushBoolean(std::string{attribute.localname()}, attribute.value(), fieldType, fieldElement);
                    break;
                case K::systemString:
                    pushString(std::string{attribute.localname()}, attribute.value(), fieldType, fieldElement);
                    break;
                case K::systemDouble:
                case K::systemInteger:
                    pushNumber(std::string{attribute.localname()}, attribute.value(), fieldType, fieldElement);
                    break;
            }
            joinTopStackElements();
        }
    }
    if (isSubObjectOfPrimitive && newObject.value.ObjectEmpty())
    {
        newObject.value.SetNull();
    }
}

void FhirSaxHandler::pushContextForSubObjectOfPrimitive(const std::string_view& localname)
{
    auto& parentItem = mStack.back();
    const auto& parentElement = parentItem.element();
    if (parentItem.subObjectOfPrimitive.IsNull())
    {
        auto& subObjectCtx = mStack.emplace_back(mStack.back().type(), parentElement,
                                                    '_' + std::string{localname}, rapidjson::kObjectType);
        subObjectCtx.isSubObjectOfPrimitive = true;
    }
    else
    {
        auto& subObjectCtx =
            mStack.emplace_back(mStack.back().type(), parentElement, '_' + std::string{localname},
                                std::move(parentItem.subObjectOfPrimitive));
        subObjectCtx.isSubObjectOfPrimitive = true;
    }
}

void FhirSaxHandler::pushBoolean(const std::string_view& name,
                          const std::string_view& value,
                          const FhirStructureDefinition& type,
                          const FhirElement& element)
{
    TVLOG(3) << name << " = " << value;
    if (value == "true")
    {
        mStack.emplace_back(type, element, std::string{name}, mResult.makeBool(true));
    }
    else if (value == "false")
    {
        mStack.emplace_back(type, element, std::string{name}, mResult.makeBool(false));
    }
    else
    {
        ErpFail(HttpStatus::BadRequest, "Invalid value for boolean type");
    }
}

void FhirSaxHandler::pushNumber(const std::string_view& name, const std::string_view& value, const FhirStructureDefinition& type, const FhirElement& element)
{
    TVLOG(3) << name << " = " << value;
    mStack.emplace_back(type, element, std::string{name}, mResult.makeNumber(value));
}

void FhirSaxHandler::pushString(const std::string_view& name, const std::string_view& value, const FhirStructureDefinition& type, const FhirElement& element)
{
    TVLOG(3) << name << " = " << value;
    mStack.emplace_back(type, element, std::string{name}, mResult.makeString(value));
}

const FhirStructureDefinition& FhirSaxHandler::systemTypeFor(const FhirStructureDefinition& primitiveType) const
{
    if (primitiveType.isSystemType())
    {
        return primitiveType;
    }
    Expect3(primitiveType.kind() == FhirStructureDefinition::Kind::primitiveType,
            "systemTypeFor called with structure of kind: " + to_string(primitiveType.kind()), std::logic_error);
    const auto* const valueElement = primitiveType.findElement(primitiveType.typeId() + ".value");
    Expect3(valueElement != nullptr, "Primitive type has no value element.", std::logic_error);
    const auto* valueType = mStructureRepo.findTypeById(valueElement->typeId());
    Expect3(valueType != nullptr, "Type not found: " + valueElement->typeId(), std::logic_error);
    Expect3(valueType->isSystemType(), "Value attribute of " + primitiveType.url() + " is not a system type", std::logic_error);
    return *valueType;
}




void FhirSaxHandler::endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    (void)localname;
    (void)prefix;
    (void)uri;
    TVLOG(3) << "endElement: " << reinterpret_cast<const char*>(localname);
    if (uri == xhtmlNamespaceUri)
    {
        endXHTMLElement(localname, prefix, uri);
        return;
    }

    while (!mStack.empty() && (mStack.back().isSubObjectOfPrimitive || mStack.back().currentArray))
    {
        TVLOG(3) << "  extra join: " << getPath();
        joinTopStackElements();
    }
    joinTopStackElements();
}


void FhirSaxHandler::joinTopStackElements()
{
    Expect3(not mStack.empty(), "Stack is empty  - cannot join.", std::logic_error);
    if (mStack.back().depth == 0)
    {
        TVLOG(3) << "  join: " << getPath();
        auto v = std::move(mStack.back());
        mStack.pop_back();
        if (!mStack.empty())
        {
            joinContext(std::move(v));
        }
        else
        {
            mResult.SetObject() = v.value;
        }
    }
    else
    {
        TVLOG(3) << "depth: " << getPath() << " = " << mStack.back().depth;
        --mStack.back().depth;
    }
}

void FhirSaxHandler::joinContext(FhirSaxHandler::Context&& context)
{
    auto& top = mStack.back();
    dropTrailingNullsIfArray(context.value);
    if (context.isSubObjectOfPrimitive)
    {
        joinSubObjectOfPrimitive(std::move(context));
    }
    else if (top.value.IsArray())
    {
        top.value.GetArray().PushBack(std::move(context.value), mResult.GetAllocator());
        top.subObjectOfPrimitive.GetArray().PushBack(std::move(context.subObjectOfPrimitive), mResult.GetAllocator());
    }
    else if (top.value.IsObject())
    {
        if (hasValue(context.value))
        {
            top.value.AddMember(asJsonValue(context.name), std::move(context.value), mResult.GetAllocator());
        }
        dropTrailingNullsIfArray(context.subObjectOfPrimitive);
        if (hasValue(context.subObjectOfPrimitive))
        {
            top.value.AddMember(asJsonValue('_' + context.name), std::move(context.subObjectOfPrimitive), mResult.GetAllocator());
        }
    }
    else
    {
        top.subObjectOfPrimitive = std::move(context.subObjectOfPrimitive);
        top.value = std::move(context.value);
    }
}

void FhirSaxHandler::joinSubObjectOfPrimitive(Context&& subObjectContext)
{
    auto& top = mStack.back();
    Expect3(subObjectContext.isSubObjectOfPrimitive, "Context is not sub-object context.", std::logic_error);
    Expect3(top.type().kind() == FhirStructureDefinition::Kind::primitiveType,
            "non-primitive types should not use subObject", std::logic_error);
    if (top.subObjectOfPrimitive.IsArray())
    {
        top.subObjectOfPrimitive.GetArray().PushBack(std::move(subObjectContext.value), mResult.GetAllocator());
    }
    else
    {
        top.subObjectOfPrimitive = std::move(subObjectContext.value);
    }
}

void FhirSaxHandler::characters ( const xmlChar* ch, int len )
{
    if (mCurrentXHTMLNode)
    {
        xmlAddChild(mCurrentXHTMLNode, xmlNewTextLen(ch, len));
    }
}


std::string FhirSaxHandler::getPath() const
{
    std::ostringstream path;
    for (const auto& item: mStack)
    {
        path << item.name;
        if (item.value.IsArray())
        {
            path << "[]";
        }
        else if (item.value.IsObject())
        {
            path << '/';
        }
    }
    return path.str();
}


template<typename T>
auto FhirSaxHandler::asJsonValue(T&& value)-> decltype(rapidjson::Value{std::forward<T>(value), std::declval<rapidjson::Document::AllocatorType>()})
{
    return rapidjson::Value{std::forward<T>(value), mResult.GetAllocator()};
}

rapidjson::Value FhirSaxHandler::asJsonValue(const std::string_view& value)
{
    return rapidjson::Value{value.data(), gsl::narrow<rapidjson::SizeType>(value.size()), mResult.GetAllocator()};
}

rapidjson::Value FhirSaxHandler::asJsonValue(const xmlChar* value)
{
    return rapidjson::Value{reinterpret_cast<const char*>(value), mResult.GetAllocator()};
}


void FhirSaxHandler::dropTrailingNullsIfArray(rapidjson::Value& value)
{
    if (!value.IsArray())
    {
        return;
    }
    auto&& arr = value.GetArray();
    while (!arr.Empty() && arr[arr.Size() - 1].IsNull())
    {
        arr.PopBack();
    }
}


void FhirSaxHandler::pushRootResource(const XmlStringView& resoureType)
{
    TVLOG(3) << "Create root resource: " << std::string_view{resoureType};
    const auto* type = mStructureRepo.findTypeById(std::string{resoureType});
    ErpExpect(type != nullptr, HttpStatus::BadRequest, "Unknown Type: "s.append(resoureType));
    ErpExpect(type->kind() == FhirStructureDefinition::Kind::resource, HttpStatus::BadRequest, "Type is not a Resource: "s.append(resoureType));
    const auto* backBone = type->findElement(type->typeId());
    Expect3(backBone != nullptr, "Element not defined: " + type->url() + "@" + type->typeId(), std::logic_error);
    auto& state = mStack.emplace_back(*type, *backBone, std::string{resoureType}, rapidjson::kObjectType);
    state.value.AddMember("resourceType", mResult.makeString(type->typeId()), mResult.GetAllocator());
}


std::string FhirSaxHandler::makeElementId(const FhirElement& baseElement, const std::string_view& name)
{
    std::string elementId;
    elementId.reserve(baseElement.name().size() + name.size() + 1);
    elementId.append(baseElement.name()).append(".").append(name);
    return elementId;
}

std::tuple<const FhirStructureDefinition&, const FhirElement&>
FhirSaxHandler::getTypeAndElement(const FhirStructureDefinition& baseType,
                           const FhirStructureDefinition* parentType,
                           const FhirElement& baseElement,
                           const std::string_view& name) const
{
    std::string elementId;
    const FhirElement* element = nullptr;
    if (baseElement.isRoot() || &baseType == mBackBoneType)
    {
        elementId = makeElementId(baseElement, name);
        if (parentType)
        {
            element = parentType->findElement(elementId);
        }
        else
        {
            element = baseType.findElement(elementId);
        }
    }
    if (!element)
    {
        const auto* newType = mStructureRepo.findTypeById(baseElement.typeId());
        Expect3(newType != nullptr, "missing type for '" + getPath().append(name) + ": " + baseElement.name(), std::logic_error);
        elementId.clear();
        elementId.reserve(baseElement.typeId().size() + name.size() + 1);
        elementId.append(baseElement.typeId()).append("."sv).append(name);
        element = newType->findElement(elementId);
    }
    ErpExpect(element != nullptr, HttpStatus::BadRequest, "Element not defined: " + getPath() + elementId);
    const auto* elementType = mStructureRepo.findTypeById(element->typeId());
    if (!elementType)
    {
        const auto& contentReference = element->contentReference();
        Expect3(not contentReference.empty(), "Cannot determin type of: " + element->name(), std::logic_error);
        const auto ref = mStructureRepo.resolveContentReference(contentReference);
        return {ref.baseType, ref.element};
    }
    Expect3(elementType != nullptr, "Could not resolve type: " + element->typeId(), std::logic_error);
    if (parentType && elementType == mBackBoneType)
    {
        elementType = parentType;
    }
    return {*elementType, *element};
}

void FhirSaxHandler::startXHTMLElement(const XmlStringView localname,
                                       int nbNamespaces,
                                       const xmlChar ** namespaces,
                                       const SaxHandler::AttributeList& attributes)
{
    const auto& parentContext = mStack.back();
    const auto& parentType = parentContext.type();
    const auto& parentElement = parentContext.element();
    ++(mStack.back().depth);
    if (!mCurrentXHTMLDoc)
    {
        const auto [fhirType, fhirElement] = getTypeAndElement(parentType, nullptr, parentElement, localname);
        ErpExpect(fhirElement.typeId() == "xhtml", HttpStatus::BadRequest,
                "element is in xhtml namespace, but should not be represented as xhtml: "
                + getPath() + "/"s.append(localname));
        mCurrentXHTMLDoc.reset(xmlNewDoc(reinterpret_cast<const xmlChar*>("1.1")));
        xmlDocSetRootElement(mCurrentXHTMLDoc.get(), xmlNewNode(nullptr, localname.xs_str()));
        mCurrentXHTMLNode = xmlDocGetRootElement(mCurrentXHTMLDoc.get());
    }
    else
    {
        Expect3(mCurrentXHTMLNode != nullptr, "When currentXHTMLDoc is set currentXHTMLNode should also be set.",
                std::logic_error);
        mCurrentXHTMLNode = xmlAddChild(mCurrentXHTMLNode, xmlNewNode(nullptr, localname.xs_str()));
    }
    const auto namespaceCount = gsl::narrow<size_t>(nbNamespaces);
    for (size_t i = 0; i < namespaceCount * 2; i += 2)
    {
        xmlNewNs(mCurrentXHTMLNode, namespaces[i + 1], namespaces[i]);
    }
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        const auto& attr = attributes.get(i);
        const auto* attrName = reinterpret_cast<const xmlChar*>(attr.localname().data());
        std::unique_ptr<xmlChar, xmlFreeFunc> value{
                xmlCharStrndup(attr.value().data(), gsl::narrow<int>(attr.value().size())), xmlFree};
        xmlNewProp(mCurrentXHTMLNode, attrName, value.get());
    }
}

void FhirSaxHandler::endXHTMLElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    (void)prefix;
    (void)uri;
    auto& depth = mStack.back().depth;
    --depth;
    if (depth > 0)
    {
        mCurrentXHTMLNode = mCurrentXHTMLNode->parent;
        Expect3(mCurrentXHTMLNode != nullptr, "No parent node: " + getPath(), std::logic_error);
        return;
    }
    UniqueXmlPtr<xmlBufferFree> buffer{xmlBufferCreate()};
    xmlNodeDump(buffer.get(), mCurrentXHTMLDoc.get(), mCurrentXHTMLNode, 0,0);
    Expect(buffer->size > 0 && buffer->content != nullptr, "XML document serialization failed.");
    std::string_view result{reinterpret_cast<const char*>(buffer->content), gsl::narrow<size_t>(buffer->use)};
    auto& topContext = mStack.back();
    topContext.value.AddMember(asJsonValue(localname), mResult.makeString(result), mResult.GetAllocator());
    mCurrentXHTMLDoc.reset();
    mCurrentXHTMLNode = nullptr;
}

