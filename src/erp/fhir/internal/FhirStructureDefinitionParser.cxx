#include "FhirStructureDefinitionParser.hxx"

#include "erp/fhir/Fhir.hxx"
#include "erp/util/Expect.hxx"
#include "erp/xml/XmlHelper.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/TLog.hxx"

#include <algorithm>
#include <array>
#include <boost/algorithm/string.hpp>
#include <map>

using namespace std::string_view_literals;
using namespace xmlHelperLiterals;

// clang-format off
// elementName -> typeId
static std::map<std::string_view, std::string_view> implementationDefinedTypes
{
    {"Bundle.entry.link"sv, Fhir::systemTypeString},
    {"Parameters.parameter.part"sv, "string"sv},
};
// clang-format on


enum class FhirStructureDefinitionParser::ElementType : uint8_t
{
    Ignored,
    entry,
    Bundle,
    resource,
    StructureDefinition,
    snapshot,
    element,
    type,
    base,
};


FhirStructureDefinitionParser::ElementInfo::ElementInfo(FhirStructureDefinitionParser::ElementType elementType, const xmlChar* theElementName)
    : type(elementType)
    , elementName(reinterpret_cast<const char*>(theElementName))
{}

FhirStructureDefinitionParser::FhirStructureDefinitionParser() = default;

FhirStructureDefinitionParser::~FhirStructureDefinitionParser() = default;


void FhirStructureDefinitionParser::startDocument()
{
}

void FhirStructureDefinitionParser::endDocument()
{
}

void FhirStructureDefinitionParser::startElement(const xmlChar* localname, const xmlChar* prefix,
                                                 const xmlChar* uri, int nbNamespaces,
                                                 const xmlChar** namespaces, int nbDefaulted,
                                                 const AttributeList& attributes)
{
    (void)prefix;
    (void)nbNamespaces;
    (void)namespaces;
    (void)nbDefaulted;
    if (mStack.empty())
    {

        Expect3(uri == Fhir::namespaceUri, "Document is not a FHIR Structure Definition or Bundle thereof.", std::logic_error);
        if (localname == "Bundle"_xs)
        {
            mStack.emplace_back(ElementType::Bundle, localname);
        }
        else if (localname == "StructureDefinition"_xs)
        {
            mStack.emplace_back(ElementType::StructureDefinition, localname);
        }
        else
        {
            Fail("Document is not a FHIR Structure Definition or Bundle thereof.");
        }
        return;
    }
    switch (mStack.back().type)
    {
        case ElementType::Ignored:
            mStack.emplace_back(ElementType::Ignored, localname);
            break;
        case ElementType::Bundle:
            enter(localname, uri, {{"entry"_xs, ElementType::entry}});
            break;
        case ElementType::entry:
            enter(localname, uri, {{"resource"_xs, ElementType::resource}});
            break;
        case ElementType::resource:
            enter(localname, uri, {{"StructureDefinition"_xs, ElementType::StructureDefinition}});
            break;
        case ElementType::StructureDefinition:
            handleStructureDefinitionSubTree(localname, uri, attributes);
            break;
        case ElementType::snapshot:
            handleSnapshotSubTree(localname, uri, attributes);
            break;
        case ElementType::element:
            handleElementSubTree(localname, uri, attributes);
            break;
        case ElementType::base:
            handleBaseSubTree(localname, uri, attributes);
            break;
        case ElementType::type:
            handleTypeSubtree(localname, uri, attributes);
            break;
    }

}



void FhirStructureDefinitionParser::endElement(const xmlChar* localname, const xmlChar* prefix,
                                               const xmlChar* uri)
{
    (void)localname;
    (void)prefix;
    (void)uri;
    Expect3(not mStack.empty(), "Stack should not be empty in endElement", std::logic_error);
    Expect3(mStack.back().elementName == reinterpret_cast<const char*>(localname), "endElement/stackValue mismatch: " + getPath(), std::logic_error);
    switch (mStack.back().type)
    {
        case ElementType::StructureDefinition:
            leaveStructureDefinition();
            break;
        case ElementType::Ignored:
        case ElementType::entry:
        case ElementType::Bundle:
        case ElementType::resource:
        case ElementType::snapshot:
            break;
        case ElementType::element:
            leaveElement();
            break;
        case ElementType::base:
        case ElementType::type:
            break;
    }
    mStack.pop_back();
}

std::list<FhirStructureDefinition> FhirStructureDefinitionParser::parse(const std::filesystem::path& filename)
{
    FhirStructureDefinitionParser parser;
    parser.SaxHandler::parseFile(filename);
    Expect3(not parser.mStructures.empty(), "Failed parsing FHIR Structure Definition", std::logic_error);
    return parser.mStructures;
}

bool FhirStructureDefinitionParser::enter(const xmlChar* localname,
                                          const xmlChar* uri,
                                          const std::initializer_list<std::tuple<const XmlStringView, ElementType>>& elements)
{
    if (uri == Fhir::namespaceUri)
    {
        for (const auto& element : elements)
        {
            using std::get;
            if (get<0>(element) == localname)
            {
                mStack.emplace_back(get<1>(element), localname);
                return true;
            }
        }
    }
    mStack.emplace_back(ElementType::Ignored, localname);
    return false;
}

void FhirStructureDefinitionParser::handleStructureDefinitionSubTree(const xmlChar* localname,
                                                          const xmlChar* uri,
                                                          const AttributeList& attributes)
{
    using namespace std::string_view_literals;
    if (uri == Fhir::namespaceUri)
    {
        if (localname == "type"_xs)
        {
            mStructureBuilder.typeId(valueAttributeFrom(attributes));
        }
        else if (localname == "url"_xs)
        {
            const auto& url = valueAttributeFrom(attributes);
            mStructureBuilder.url(url);
            TVLOG(3) << "loading FHIR Structure Definition for: " << url;
        }
        else if (localname == "baseDefinition"_xs)
        {
            mStructureBuilder.baseDefinition(valueAttributeFrom(attributes));
        }
        else if (localname == "derivation"_xs)
        {
            mStructureBuilder.derivation(stringToDerivation(valueAttributeFrom(attributes)));
        }
        else if (localname == "experimental"_xs)
        {
            auto value = valueAttributeFrom(attributes);
            if (value == "true")
            {
                mExperimental = true;
            }
            else
            {
                Expect3(value == "false", "value invalid value for experimental in '" + getPath() + "': " + value, std::logic_error);
            }
        }
        else if (localname == "kind"_xs)
        {
            mStructureBuilder.kind(stringToKind(valueAttributeFrom(attributes)));
        }
        else
        {
            enter(localname, uri, {{"snapshot"_xs, ElementType::snapshot}});
            return;
        }
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void FhirStructureDefinitionParser::leaveStructureDefinition()
{
    if (! mExperimental)
    {
        mStructures.emplace_back(mStructureBuilder.getAndReset());
        TVLOG(3) << "loaded: " << mStructures.back();
    }
    else
    {
        auto expStructure = mStructureBuilder.getAndReset();
        TVLOG(3) << "ignoring experimental Structure: " << expStructure.url();
        mExperimental = false;
    }
}

void FhirStructureDefinitionParser::handleSnapshotSubTree(const xmlChar* localname,
                                               const xmlChar* uri,
                                               const SaxHandler::AttributeList& attributes)
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    Expect3(localname == "element"_xs,
            "unexpected xml element '"s.append(reinterpret_cast<const char*>(localname)) + "' in " + getPath(),
            std::logic_error);

    mStack.emplace_back(ElementType::element, localname);
    ensureFhirNamespace(uri);

    auto id = attributes.findAttribute("id"sv);
    Expect3(id.has_value(), "Missing id attribute on: " + getPath(), std::logic_error);
    mElementBuilder.name(std::string{id->value()});
}

void FhirStructureDefinitionParser::handleElementSubTree(const xmlChar* localname, const xmlChar* uri, const SaxHandler::AttributeList& attributes)
{
    ensureFhirNamespace(uri);
    bool hasEntered = enter(localname, uri, {{"type"_xs, ElementType::type}, {"base"_xs, ElementType::base}});
    if (!hasEntered)
    {
        if (localname == "representation"_xs)
        {
            mElementBuilder.representation(stringToRepresentation(valueAttributeFrom(attributes)));
        }
        else if (localname == "contentReference"_xs)
        {
            mElementBuilder.contentReference(valueAttributeFrom(attributes));
        }
    }
}

void FhirStructureDefinitionParser::leaveElement()
{
    static const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    if (mElementTypes.size() == 1)
    {
        mElementBuilder.typeId(mElementTypes.front());
    }
    auto element = mElementBuilder.getAndReset();
    if (mElementTypes.empty())
    {
        auto implType = implementationDefinedTypes.find(element.name());
        if (implType != implementationDefinedTypes.end())
        {
            mStructureBuilder.addElement(
                FhirElement::Builder{element}
                    .typeId(std::string{implType->second}).getAndReset());
            return;
        }
    }
    if (! boost::ends_with(element.name(), Fhir::typePlaceholder))
    {
        Expect3(mElementTypes.size() <= 1,
                "More than one element type for element without placeholder: " + getPath(), std::logic_error);
        mStructureBuilder.addElement(std::move(element));
        mElementTypes.clear();
        return;
    }
    Expect3(not mElementTypes.empty(), "No type for element with placeholder: " + getPath(), std::logic_error);
    auto namePrefix = element.name().substr(0, element.name().size() - Fhir::typePlaceholder.size());
    for (const auto& type: mElementTypes)
    {
        Expect3(not type.empty(), "Empty type id: " + getPath(), std::logic_error);
        std::string nameSuffix = type;
        nameSuffix[0] = ctype.toupper(nameSuffix[0]);
        mStructureBuilder.addElement(
                FhirElement::Builder{element}
                    .name(namePrefix + nameSuffix)
                    .typeId(type).getAndReset());
    }
    mElementTypes.clear();
}


void FhirStructureDefinitionParser::handleBaseSubTree(const xmlChar* localname, const xmlChar* uri, const SaxHandler::AttributeList& attributes)
{
    using namespace std::string_literals;

    mStack.emplace_back(ElementType::Ignored, localname);
    ensureFhirNamespace(uri);

    if (localname == "max"_xs)
    {
        auto value = valueAttributeFrom(attributes);
        if (value == "*")
        {
            mElementBuilder.isArray(true);
        }
        else if (value == "1")
        {
            mElementBuilder.isArray(false);
        }
        else
        {
            Fail("Unsupported value '" + value + "' in " + getPath());
        }
    }
}

void FhirStructureDefinitionParser::handleTypeSubtree(const xmlChar* localname, const xmlChar* uri, const SaxHandler::AttributeList& attributes)
{
    mStack.emplace_back(ElementType::Ignored, localname);
    ensureFhirNamespace(uri);
    if (localname == "code"_xs)
    {
        mElementTypes.push_back(valueAttributeFrom(attributes));
    }
}


std::string FhirStructureDefinitionParser::getPath()
{
    std::ostringstream result;
    for (const auto& info: mStack)
    {
        result  << "/" << info.elementName;
    }
    return result.str();
}

std::string FhirStructureDefinitionParser::valueAttributeFrom(const AttributeList& attributes)
{
    using namespace std::string_view_literals;
    auto value = attributes.findAttribute("value"sv);
    Expect3(value.has_value(), "Missing value attribute on " + getPath(), std::logic_error);
    return std::string{value->value()};
}

void FhirStructureDefinitionParser::ensureFhirNamespace(const xmlChar* uri)
{
    using namespace std::string_literals;
    Expect3(uri == Fhir::namespaceUri,
        "unexpected xml uri '"s.append(reinterpret_cast<const char*>(uri)) + "' in " + getPath(),
        std::logic_error);
}

