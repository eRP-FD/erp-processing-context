#include "FhirJsonToXmlConverter.hxx"

#include <boost/algorithm/string.hpp>
#include <rapidjson/document.h>

#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirStructureDefinition.hxx"
#include "erp/fhir/FhirStructureRepository.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/xml/XmlHelper.hxx"

using namespace std::string_literals;
using namespace xmlHelperLiterals;

UniqueXmlDocumentPtr FhirJsonToXmlConverter::jsonToXml(const FhirStructureRepository& repo,
                                                       const model::NumberAsStringParserDocument& jsonDoc)
{
    FhirJsonToXmlConverter converter{repo};
    converter.convertResource(nullptr, jsonDoc);
    return std::move(converter.mResultDoc);
}


FhirJsonToXmlConverter::FhirJsonToXmlConverter(const FhirStructureRepository& repo)
    : mStructureRepository(repo), mResultDoc(xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0")))
{
}

//NOLINTNEXTLINE[misc-no-recursion]
void FhirJsonToXmlConverter::convertResource(xmlNode* parent, const rapidjson::Value& resource)
{
    static const rapidjson::Pointer resourceTypePointer{
        model::resource::ElementName::path(model::resource::elements::resourceType)};
    const auto* resourceTypeValue = resourceTypePointer.Get(resource);
    ModelExpect(resourceTypeValue != nullptr, "resourceType not set.");
    ModelExpect(model::NumberAsStringParserDocument::valueIsString(*resourceTypeValue), "resourceType is not string.");
    std::string resourceTypeId{model::NumberAsStringParserDocument::getStringValueFromValue(resourceTypeValue)};
    TVLOG(3) << "Create Resource: " << resourceTypeId;
    const auto* resourceType = mStructureRepository.findTypeById(std::string{resourceTypeId});
    ModelExpect(resourceType != nullptr, "Unknown resource type: "s.append(resourceTypeId));
    UniqueXmlNodePtr resourceNode{
        xmlNewNode(mFhirNamespace, reinterpret_cast<const xmlChar*>(resourceTypeId.data()))};
    if (parent)
    {
        (void)convertStructure(*xmlAddChild(parent, resourceNode.release()), *resourceType, 0, resource);
    }
    else
    {
        initRoot(std::move(resourceNode));
        (void)convertStructure(*mRootNode, *resourceType, 0, resource);
    }
}

void FhirJsonToXmlConverter::initRoot(UniqueXmlNodePtr rootNode)
{
    Expect3(mRootNode == nullptr, "initRoot called, but root node already initialized.",
            std::logic_error);
    mRootNode = rootNode.get();
    Expect3(xmlDocSetRootElement(mResultDoc.get(), rootNode.release()) == nullptr,
            "no previous root node expected.", std::logic_error);
    mFhirNamespace = xmlNewNs(mRootNode, Fhir::namespaceUri.xs_str(), nullptr);
}

//NOLINTNEXTLINE[misc-no-recursion]
size_t FhirJsonToXmlConverter::convertStructure(xmlNode& targetNode,
                                              const FhirStructureDefinition& fhirType,
                                              size_t fhirElementIndex, const rapidjson::Value& object)
{
    const auto& fhirElements = fhirType.elements();
    const std::string prefix = fhirElements.at(fhirElementIndex).name() + '.';
    ++fhirElementIndex;
    while (fhirElementIndex < fhirElements.size())
    {
        const auto& fhirElementInfo = fhirElements[fhirElementIndex];
        const auto& fhirElementName = fhirElementInfo.name();
        TVLOG(3) << "elementName = " << fhirElementName;
        if (! boost::starts_with(fhirElementName, prefix))
        {
            // BackboneElement completed - return to parent structure
            return fhirElementIndex;
        }
        Expect3(fhirElementName.find_first_of('.', prefix.size()) == std::string::npos,
                "Dot (.) in element name: " + fhirElementName, std::logic_error);
        const auto& memberName = fhirElementName.substr(prefix.size());
        const auto& jsonMember = object.FindMember(memberName);
        const auto& jsonSubObjectOfPrimary = object.FindMember('_' + memberName);

        if (jsonMember == object.MemberEnd())
        {
            ModelExpect(jsonSubObjectOfPrimary == object.MemberEnd(),
                        "found sub-object, but primary value is not defined: "s.append(memberName));
            fhirElementIndex = skipElement(fhirType, fhirElementIndex);
        }
        else if (jsonSubObjectOfPrimary != object.MemberEnd())
        {
            fhirElementIndex = convertMember(targetNode, memberName, fhirType, fhirElementIndex, jsonMember->value,
                                             jsonSubObjectOfPrimary->value);
        }
        else
        {
            fhirElementIndex = convertMember(targetNode, memberName, fhirType, fhirElementIndex, jsonMember->value,
                                             rapidjson::Value{rapidjson::kNullType});
        }
    }
    return fhirElementIndex;
}

//NOLINTNEXTLINE[misc-no-recursion]
size_t FhirJsonToXmlConverter::convertMember(xmlNode& targetNode, const std::string& memberName,
                                             const FhirStructureDefinition& fhirParentType, size_t fhirElementIndex,
                                             const rapidjson::Value& jsonMember,
                                             const rapidjson::Value& subObjectOfPrimary)
{
    const auto& fhirElements = fhirParentType.elements();
    const auto& fhirElementInfo = fhirElements[fhirElementIndex];
    if (fhirElementInfo.typeId() == "xhtml")
    {
        return convertXHTMLMember(targetNode, memberName, fhirParentType, fhirElementIndex, jsonMember);
    }
    const auto* fhirElementType = mStructureRepository.findTypeById(fhirElementInfo.typeId());
    if (fhirElementType != nullptr)
    {
        return convertMemberWithType(targetNode, memberName, fhirParentType, *fhirElementType, fhirElementIndex,
                                     jsonMember, subObjectOfPrimary);
    }
    const auto& contentReference = fhirElementInfo.contentReference();
    Expect3(not contentReference.empty(), "Cannot determin type of: " + fhirElementInfo.name(), std::logic_error);
    auto ref = mStructureRepository.resolveContentReference(fhirElementInfo.contentReference());
    (void)convertMemberWithType(targetNode, memberName, ref.baseType, ref.elementType, ref.elementIndex, jsonMember,
                                subObjectOfPrimary);
    return skipElement(fhirParentType, fhirElementIndex);
}

//NOLINTNEXTLINE[misc-no-recursion]
size_t FhirJsonToXmlConverter::convertMemberWithType(xmlNode& targetNode, const std::string& memberName,
                                                     const FhirStructureDefinition& fhirParentType,
                                                     const FhirStructureDefinition& fhirElementType,
                                                     size_t fhirElementIndex, const rapidjson::Value& jsonMember,
                                                     const rapidjson::Value& subObjectOfPrimary)
{
    const auto& fhirElements = fhirParentType.elements();
    const auto& fhirElementInfo = fhirElements[fhirElementIndex];
    const auto& fhirElementName = fhirElementInfo.name();
    xmlNode* fieldNode = &targetNode;
    if (fhirElementType.kind() == FhirStructureDefinition::Kind::resource)
    {
        fieldNode = xmlAddChild(&targetNode,
                                    xmlNewNode(mFhirNamespace, reinterpret_cast<const xmlChar*>(memberName.data())));
        Expect3(fieldNode != nullptr, "Failed add child: " + fhirElementName, std::logic_error);
    }
    if (fhirElementInfo.isArray())
    {
        static const rapidjson::Value jsonNull{rapidjson::kNullType};
        ModelExpect(jsonMember.IsArray(), "Expected json array: " + fhirElementName);
        ModelExpect(subObjectOfPrimary.IsNull() ||
                   (subObjectOfPrimary.IsArray() && subObjectOfPrimary.Size() == jsonMember.Size()),
                   "number of elements in sub-object array doesn't match primary array.");
        for (rapidjson::SizeType idx = 0; idx < jsonMember.Size(); ++idx)
        {
            const auto& subObject = subObjectOfPrimary.IsNull()?jsonNull:subObjectOfPrimary.GetArray()[idx];
            (void) convertSingleMember(*fieldNode, memberName, fhirParentType, fhirElementIndex, jsonMember.GetArray()[idx],
                                       subObject);
        }
        fhirElementIndex = skipElement(fhirParentType, fhirElementIndex);
    }
    else
    {
        ModelExpect(not jsonMember.IsArray(), "Unexpected json array: " + fhirElementName);
        fhirElementIndex = convertSingleMember(*fieldNode, memberName, fhirParentType, fhirElementIndex, jsonMember,
                                               subObjectOfPrimary);
    }
    return fhirElementIndex;
}

//NOLINTNEXTLINE[misc-no-recursion]
size_t FhirJsonToXmlConverter::convertSingleMember(xmlNode& targetNode, const std::string& memberName,
                                                   const FhirStructureDefinition& fhirType,
                                                   size_t fhirElementIndex, const rapidjson::Value& jsonObject,
                                                   const rapidjson::Value& subObjectOfPrimary)
{
    const auto& fhirElements = fhirType.elements();
    const auto& fhirElementInfo = fhirElements[fhirElementIndex];
    const auto& fhirElementName = fhirElementInfo.name();
    const auto* fhirElementType = mStructureRepository.findTypeById(fhirElementInfo.typeId());
    Expect3(fhirElementType != nullptr, "Type not found for " + fhirElementName + ": " + fhirElementInfo.typeId(),
            std::logic_error);
    switch (fhirElementType->kind())
    {
        case FhirStructureDefinition::Kind::resource:
        {
            ModelExpect(jsonObject.IsObject(),
                        "Expected json object: " + fhirElementName);
            convertResource(&targetNode, jsonObject);
            ++fhirElementIndex;
            break;
        }
        case FhirStructureDefinition::Kind::complexType:
        {
            ModelExpect(jsonObject.IsObject(),
                        "Expected json object: " + fhirElementName);
            auto* subNode = xmlAddChild(&targetNode, xmlNewNode(mFhirNamespace, reinterpret_cast<const xmlChar*>(memberName.c_str())));
            if (fhirElementType->typeId() == "BackboneElement")
            {
                TVLOG(3) << "BackboneElement: " + fhirElementName;
                fhirElementIndex = convertStructure(*subNode, fhirType, fhirElementIndex, jsonObject);
            }
            else
            {
                TVLOG(3) << "enterType: " + fhirElementName;
                (void)convertStructure(*subNode, *fhirElementType, 0, jsonObject);
                ++fhirElementIndex;
            }
            break;
        }
        case FhirStructureDefinition::Kind::primitiveType:
        case FhirStructureDefinition::Kind::systemBoolean:
        case FhirStructureDefinition::Kind::systemDouble:
        case FhirStructureDefinition::Kind::systemInteger:
        case FhirStructureDefinition::Kind::systemString:
        {
            TVLOG(3) << fhirElementType->typeId() << fhirElementName;
            const auto& value = valueString(fhirElementName, jsonObject);
            if (fhirElementInfo.representation() == FhirElement::Representation::xmlAttr)
            {
                xmlNewProp(&targetNode, reinterpret_cast<const xmlChar*>(memberName.c_str()), reinterpret_cast<const xmlChar*>(value.c_str()));
            }
            else
            {
                auto* primitiveNode = xmlAddChild(&targetNode, xmlNewNode(mFhirNamespace, reinterpret_cast<const xmlChar*>(memberName.c_str())));
                xmlNewProp(primitiveNode, "value"_xs.xs_str(), reinterpret_cast<const xmlChar*>(value.c_str()));
                if (!subObjectOfPrimary.IsNull())
                {
                    (void)convertStructure(*primitiveNode, *fhirElementType, 0, subObjectOfPrimary);
                }
            }
            ++fhirElementIndex;
            break;
        }
        case FhirStructureDefinition::Kind::logical:
            Fail("Logical StructureDefinitions not supported.");
    }
    return fhirElementIndex;
}

size_t FhirJsonToXmlConverter::convertXHTMLMember(xmlNode& targetNode, const std::string& memberName,
                                                  const FhirStructureDefinition& fhirParentType,
                                                  size_t fhirElementIndex, const rapidjson::Value& jsonMember)
{
    const auto& parentElement = fhirParentType.elements()[fhirElementIndex];
    const auto& elementName = parentElement.name();

    ModelExpect(model::NumberAsStringParserDocument::valueIsString(jsonMember),
                "member of type xhtml is not a string: " + elementName);
    auto xhtmlString = model::NumberAsStringParserDocument::getStringValueFromValue(&jsonMember);
    UniqueXmlDocumentPtr xhtmlDoc{xmlParseMemory(xhtmlString.data(), gsl::narrow_cast<int>(xhtmlString.size()))};
    ModelExpect(xhtmlDoc != nullptr, "parse error in xhtml element: " + elementName);
    auto* xhtmlRoot = xmlDocGetRootElement(xhtmlDoc.get());
    Expect3(xhtmlRoot != nullptr, "no root element in xhtml element: " + elementName, std::logic_error);
    ModelExpect(xhtmlRoot->name == XmlStringView{memberName},
                "xhtml root element name doesn't match member name: " + elementName);
    xmlUnlinkNode(xhtmlRoot);
    if (xhtmlRoot->ns)
    {
        ModelExpect(xhtmlRoot->ns->href == xhtmlNamespaceUri,
                    "Invalid namespace on xhtml root element: " + elementName);
    }
    else
    {
        xmlNewNs(xhtmlRoot, xhtmlNamespaceUri.xs_str(), nullptr);
    }
    xmlAddChild(&targetNode, xhtmlRoot);
    return skipElement(fhirParentType, fhirElementIndex);
}

std::string FhirJsonToXmlConverter::valueString(const std::string_view& elementName, const rapidjson::Value& member)
{

    switch (member.GetType())
    {
        case rapidjson::kNullType:
            return "null"s;
        case rapidjson::kFalseType:
            return "false"s;
        case rapidjson::kTrueType:
            return "true"s;
        case rapidjson::kStringType:
            return std::string{model::NumberAsStringParserDocument::getStringValueFromValue(&member)};
        case rapidjson::kNumberType:
            Fail("NumberAsStringParserDocument should not contain numbers: "s.append(elementName));
        case rapidjson::kObjectType:
            Fail("JSON member may not be Object Type: "s.append(elementName));
        case rapidjson::kArrayType:
            Fail("JSON member may not be Array Type: "s.append(elementName));
    }
    Fail("Unknown JSON-Type: "s.append(elementName));
}

size_t FhirJsonToXmlConverter::skipElement(const FhirStructureDefinition& type, size_t fhirElementIndex)
{
    const auto& fhirElements = type.elements();
    const std::string prefix = fhirElements.at(fhirElementIndex).name() + '.';
    TVLOG(3) << "skip: " << prefix;
    ++fhirElementIndex;
    for (; fhirElementIndex < fhirElements.size(); ++fhirElementIndex)
    {
        const auto& fhirElementInfo = fhirElements[fhirElementIndex];
        const auto& fhirElementName = fhirElementInfo.name();
        if (! boost::starts_with(fhirElementName, prefix))
        {
            return fhirElementIndex;
        }
        TVLOG(3) << "skip: " << fhirElementName;
    }
    return fhirElementIndex;
}
