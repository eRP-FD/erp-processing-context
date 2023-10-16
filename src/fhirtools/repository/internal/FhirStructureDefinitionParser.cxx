/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirStructureDefinitionParser.hxx"

#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <unordered_set>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/util/XmlHelper.hxx"

using namespace std::string_view_literals;
using namespace xmlHelperLiterals;
using namespace fhirtools;


class FhirStructureDefinitionParser::ElementInfo
{
public:
    ElementInfo(ElementType elementType, const xmlChar* elementName);
    ElementType type;
    std::string elementName;
    std::unique_ptr<FhirValue::Builder> valueBuilder;
};

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
    constraint,
    slicing,
    discriminator,
    pattern,
    fixed,
    CodeSystem,
    ValueSet,
    Concept,
    compose,
    composeInclude,
    composeIncludeConcept,
    composeIncludeFilter,
    composeExclude,
    composeExcludeConcept,
    expansion,
    expansionContains,
    binding
};

FhirStructureDefinitionParser::ElementInfo::ElementInfo(FhirStructureDefinitionParser::ElementType elementType,
                                                        const xmlChar* theElementName)
    : type(elementType)
    , elementName(reinterpret_cast<const char*>(theElementName))
{
}

FhirStructureDefinitionParser::FhirStructureDefinitionParser() = default;

FhirStructureDefinitionParser::~FhirStructureDefinitionParser() = default;


void FhirStructureDefinitionParser::startDocument()
{
}

void FhirStructureDefinitionParser::endDocument()
{
}

void FhirStructureDefinitionParser::startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                                                 int nbNamespaces, const xmlChar** namespaces, int nbDefaulted,
                                                 const AttributeList& attributes)
{
    (void) prefix;
    (void) nbNamespaces;
    (void) namespaces;
    (void) nbDefaulted;
    if (mStack.empty())
    {

        Expect3(uri == constants::namespaceUri,
                "Document is not a FHIR Structure Definition or Bundle thereof: " + currentFile.string(),
                std::logic_error);
        if (localname == "Bundle"_xs)
        {
            mStack.emplace_back(ElementType::Bundle, localname);
        }
        else if (localname == "StructureDefinition"_xs)
        {
            mStack.emplace_back(ElementType::StructureDefinition, localname);
        }
        else if (localname == "CodeSystem"_xs)
        {
            mStack.emplace_back(ElementType::CodeSystem, localname);
        }
        else if (localname == "ValueSet"_xs)
        {
            mStack.emplace_back(ElementType::ValueSet, localname);
        }
        else if (localname == "NamingSystem"_xs || localname == "OperationDefinition"_xs ||
                 localname == "ImplementationGuide"_xs || localname == "ConceptMap"_xs ||
                 localname == "SearchParameter"_xs)
        {
            TVLOG(2) << "skipping unneeded element: " + std::string{XmlStringView{localname}};
            mStack.emplace_back(ElementType::Ignored, localname);
        }
        else
        {
            Fail("Unsupported Document type: " + std::string{XmlStringView{localname}});
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
            if (localname == "StructureDefinition"_xs)
            {
                enter(localname, uri, {{"StructureDefinition"_xs, ElementType::StructureDefinition}});
            }
            else if (localname == "CodeSystem"_xs)
            {
                enter(localname, uri, {{"CodeSystem"_xs, ElementType::CodeSystem}});
            }
            else
            {
                enter(localname, uri, {{"ValueSet"_xs, ElementType::ValueSet}});
            }
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
        case ElementType::constraint:
            handleConstraintSubtree(localname, uri, attributes);
            break;
        case ElementType::slicing:
            handleSlicingSubtree(localname, uri, attributes);
            break;
        case ElementType::discriminator:
            handleDiscriminatorSubtree(localname, uri, attributes);
            break;
        case ElementType::pattern:
        case ElementType::fixed:
            handleValueSubtree(XmlStringView{localname}, uri, attributes);
            break;
        case ElementType::CodeSystem:
            handleCodeSystemSubtree(localname, uri, attributes);
            break;
        case ElementType::Concept:
            handleConceptSubtree(localname, uri, attributes);
            break;
        case ElementType::ValueSet:
            handleValueSetSubtree(localname, uri, attributes);
            break;
        case ElementType::compose:
            handleComposeSubtree(localname, uri, attributes);
            break;
        case ElementType::composeInclude:
            handleComposeIncludeSubtree(localname, uri, attributes);
            break;
        case ElementType::composeIncludeConcept:
            handleComposeIncludeConceptSubtree(localname, uri, attributes);
            break;
        case ElementType::composeIncludeFilter:
            handleComposeIncludeFilterSubtree(localname, uri, attributes);
            break;
        case ElementType::composeExclude:
            handleComposeExcludeSubtree(localname, uri, attributes);
            break;
        case ElementType::composeExcludeConcept:
            handleComposeExcludeConceptSubtree(localname, uri, attributes);
            break;
        case ElementType::expansion:
            handleExpansionSubtree(localname, uri, attributes);
            break;
        case ElementType::expansionContains:
            handleExpansionContainsSubtree(localname, uri, attributes);
            break;
        case ElementType::binding:
            handleBindingSubtree(localname, uri, attributes);
            break;
    }
}


void FhirStructureDefinitionParser::endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    (void) localname;
    (void) prefix;
    (void) uri;
    Expect3(not mStack.empty(), "Stack should not be empty in endElement", std::logic_error);
    Expect3(mStack.back().elementName == reinterpret_cast<const char*>(localname),
            "endElement/stackValue mismatch: " + getPath() + ": " + mStack.back().elementName +
                " != " + reinterpret_cast<const char*>(localname),
            std::logic_error);
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
        case ElementType::compose:
        case ElementType::composeInclude:
        case ElementType::composeIncludeConcept:
        case ElementType::composeIncludeFilter:
        case ElementType::composeExclude:
        case ElementType::composeExcludeConcept:
        case ElementType::expansion:
        case ElementType::expansionContains:
        case ElementType::binding:
            break;
        case ElementType::Concept:
            leaveConcept();
            break;
        case ElementType::element:
            leaveElement();
            break;
        case ElementType::base:
        case ElementType::type:
            break;
        case ElementType::constraint:
            leaveConstraint();
            break;
        case ElementType::slicing:
            break;
        case ElementType::discriminator:
            leaveDiscriminator();
            break;
        case ElementType::pattern:
        case ElementType::fixed:
            leaveValue();
            break;
        case ElementType::CodeSystem:
            leaveCodeSystem();
            break;
        case ElementType::ValueSet:
            leaveValueSet();
            break;
    }
    mStack.pop_back();
}

FhirStructureDefinitionParser::ParseResult FhirStructureDefinitionParser::parse(const std::filesystem::path& filename)
{
    FhirStructureDefinitionParser parser;
    parser.currentFile = filename;
    parser.SaxHandler::parseFile(filename);
    return std::make_tuple(std::move(parser.mStructures), std::move(parser.mCodeSystems), std::move(parser.mValueSets));
}

bool FhirStructureDefinitionParser::enter(
    const xmlChar* localname, const xmlChar* uri,
    const std::initializer_list<std::tuple<const XmlStringView, ElementType>>& elements)
{
    if (uri == constants::namespaceUri)
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

void FhirStructureDefinitionParser::handleStructureDefinitionSubTree(const xmlChar* localname, const xmlChar* uri,
                                                                     const AttributeList& attributes)
{
    using namespace std::string_view_literals;
    if (uri == constants::namespaceUri)
    {
        if (localname == "type"_xs)
        {
            mStructureType = valueAttributeFrom(attributes);
            mStructureBuilder.typeId(mStructureType);
        }
        else if (localname == "name"_xs)
        {
            mStructureBuilder.name(valueAttributeFrom(attributes));
        }
        else if (localname == "url"_xs)
        {
            mStructureUrl = valueAttributeFrom(attributes);
            mStructureBuilder.url(mStructureUrl);
            TVLOG(4) << "loading FHIR Structure Definition for: " << mStructureUrl;
        }
        else if (localname == "version"_xs)
        {
            mStructureBuilder.version(Version{valueAttributeFrom(attributes)});
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
                Expect3(value == "false", "value invalid value for experimental in '" + getPath() + "': " + value,
                        std::logic_error);
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
    //    if (! mExperimental)
    {
        mStructures.emplace_back(mStructureBuilder.getAndReset());
        TVLOG(4) << "loaded: " << mStructures.back();
    }
    //    else
    //    {
    //        auto expStructure = mStructureBuilder.getAndReset();
    //        TVLOG(3) << "ignoring experimental Structure: " << expStructure.url();
    //        mExperimental = false;
    //    }
}

void FhirStructureDefinitionParser::handleSnapshotSubTree(const xmlChar* localname, const xmlChar* uri,
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
    auto idValue = id->value();
    bool prefixedWithType = idValue.starts_with(mStructureType) &&
                            (idValue.size() <= mStructureType.size() || idValue[mStructureType.size()] == '.');
    Expect3(prefixedWithType,
            ("Element id doesn't start with StructureType ('" + mStructureType + "'): ").append(idValue),
            std::logic_error);
    mElementBuilder.name(std::string{idValue});
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void FhirStructureDefinitionParser::handleElementSubTree(const xmlChar* localname, const xmlChar* uri,
                                                         const SaxHandler::AttributeList& attributes)
{
    using namespace std::string_literals;
    static constexpr std::string_view fixed{"fixed"};
    static constexpr std::string_view pattern{"pattern"};
    ensureFhirNamespace(uri);
    if (localname == "slicing"_xs)
    {
        Expect3(! mSlicingBuilder.has_value(), "duplicate slicing element.", std::logic_error);
        mSlicingBuilder.emplace();
        mStack.emplace_back(ElementType::slicing, localname);
        return;
    }
    std::string_view localnameView{XmlStringView{localname}};
    if (localnameView.starts_with(fixed))
    {
        enterValueSubtree(ElementType::fixed, fixed, localname, attributes);
        return;
    }
    if (localnameView.starts_with(pattern))
    {
        enterValueSubtree(ElementType::pattern, pattern, localname, attributes);
        return;
    }
    bool hasEntered = enter(localname, uri,
                            {{"type"_xs, ElementType::type},
                             {"base"_xs, ElementType::base},
                             {"constraint"_xs, ElementType::constraint},
                             {"binding"_xs, ElementType::binding}});
    if (hasEntered)
    {
        return;
    }
    if (localname == "representation"_xs)
    {
        mElementBuilder.representation(stringToRepresentation(valueAttributeFrom(attributes)));
    }
    else if (localname == "contentReference"_xs)
    {
        mElementBuilder.contentReference(valueAttributeFrom(attributes));
    }
    else if (localname == "max"_xs || localname == "min"_xs || localname ==  "maxLength"_xs)
    {
        const auto val = valueAttributeFrom(attributes);
        if (val == "*" && localname == "max"_xs)
        {
            mElementBuilder.cardinalityMax(std::nullopt);
        }
        else
        {
            uint32_t uintVal{};
            const auto *end = val.data() + val.size();
            const auto res = std::from_chars(val.data(), end, uintVal);
            Expect3(res.ec == std::errc{} && res.ptr == end,
                    "failed to convert value for "s.append(localnameView) + ": " +
                        (res.ec == std::errc{} ? "trailing characters" : std::make_error_code(res.ec).message()) +
                        " in '" + val + "'",
                    std::logic_error);
            if (localname == "min"_xs)
            {
                mElementBuilder.cardinalityMin(uintVal);
            }
            else if (localname == "max"_xs)
            {
                mElementBuilder.cardinalityMax(uintVal);
            }
            else if (localname == "maxLength"_xs)
            {
                mElementBuilder.maxLength(uintVal);
            }
        }
    }
    else if (localname == "minValueInteger"_xs || localname == "maxValueInteger"_xs)
    {
        int intVal{};
        const auto val = valueAttributeFrom(attributes);
        const auto* end = val.data() + val.size();
        const auto res = std::from_chars(val.data(), end, intVal);
        Expect3(res.ec == std::errc{} && res.ptr == end,
                "failed to convert value for "s.append(localnameView) + ": " +
                    (res.ec == std::errc{} ? "trailing characters" : std::make_error_code(res.ec).message()) +
                    " in '" + val + "'",
                std::logic_error);
        if (localname == "minValueInteger"_xs)
        {
            mElementBuilder.minValueInteger(intVal);
        }
        else if (localname == "maxValueInteger"_xs)
        {
            mElementBuilder.maxValueInteger(intVal);
        }
    }
    else if (localname == "minValueDecimal"_xs)
    {
        mElementBuilder.minValueDecimal(valueAttributeFrom(attributes));
    }
    else if (localname =="maxValueDecimal"_xs)
    {
        mElementBuilder.maxValueDecimal(valueAttributeFrom(attributes));
    }
    else if (localnameView.starts_with("minValue") || localnameView.starts_with("maxValue"))
    {
        // more min/maxValue[x] cases can be implemented when needed.
        Fail("unsupported: " + std::string(localnameView));
    }
}

void FhirStructureDefinitionParser::leaveElement()
{
    //     static const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    if (mElementTypes.size() == 1)
    {
        mElementBuilder.typeId(mElementTypes.front());
    }
    if (mSlicingBuilder)
    {
        mElementBuilder.slicing(mSlicingBuilder->getAndReset());
        mSlicingBuilder.reset();
    }
    auto element = mElementBuilder.getAndReset();

    if (element->hasBinding())
    {
        // eld-11: Binding can only be present for coded elements, string, and uri
        // binding.empty() or type.code.empty() or type.select((code = 'code') or (code = 'Coding') or (code='CodeableConcept') or (code = 'Quantity') or (code = 'string') or (code = 'uri')).exists()
        static std::unordered_set<std::string_view> validBindingTypeIds{"code",     "Coding", "CodeableConcept",
                                                                        "Quantity", "string", "uri"};
        bool hasValidBindingType{false};
        for (const auto& elementType : mElementTypes)
        {
            if (validBindingTypeIds.contains(elementType))
            {
                hasValidBindingType = true;
                break;
            }
        }
        if (! hasValidBindingType)
        {
            TLOG(WARNING) << "Element " << element->originalName() << " of " << mStructureUrl
                          << " has defined a binding but does not contain a valid binding type";
        }
    }
    Expect3(element != nullptr, "element must not be null.", std::logic_error);
    std::list<std::string> elementTypes;
    elementTypes.swap(mElementTypes);
    mStructureBuilder.addElement(std::move(element), std::move(elementTypes));
    mElementTypes.clear();
}


void FhirStructureDefinitionParser::handleBaseSubTree(const xmlChar* localname, const xmlChar* uri,
                                                      const SaxHandler::AttributeList& attributes)
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

void FhirStructureDefinitionParser::handleTypeSubtree(const xmlChar* localname, const xmlChar* uri,
                                                      const SaxHandler::AttributeList& attributes)
{
    mStack.emplace_back(ElementType::Ignored, localname);
    ensureFhirNamespace(uri);
    if (localname == "code"_xs)
    {
        mElementTypes.emplace_back(valueAttributeFrom(attributes));
    }
    else if (localname == "profile"_xs)
    {
        mElementBuilder.addProfile(valueAttributeFrom(attributes));
    }
    else if (localname == "targetProfile"_xs)
    {
        mElementBuilder.addTargetProfile(valueAttributeFrom(attributes));
    }
}

void FhirStructureDefinitionParser::handleConstraintSubtree(const xmlChar* localname, const xmlChar* uri,
                                                            const SaxHandler::AttributeList& attributes)
{
    mStack.emplace_back(ElementType::Ignored, localname);
    ensureFhirNamespace(uri);
    if (localname == "key"_xs)
    {
        mConstraintBuilder.key(valueAttributeFrom(attributes));
    }
    else if (localname == "severity"_xs)
    {
        mConstraintBuilder.severity(valueAttributeFrom(attributes));
    }
    else if (localname == "human"_xs)
    {
        mConstraintBuilder.human(valueAttributeFrom(attributes));
    }
    else if (localname == "expression"_xs)
    {
        mConstraintBuilder.expression(valueAttributeFrom(attributes));
    }
}

void FhirStructureDefinitionParser::leaveConstraint()
{
    auto constraint = mConstraintBuilder.getAndReset();
    mElementBuilder.addConstraint(constraint);
}

void FhirStructureDefinitionParser::handleSlicingSubtree(const xmlChar* localname, const xmlChar* uri,
                                                         const SaxHandler::AttributeList& attributes)
{
    bool hasEntered = enter(localname, uri, {{"discriminator"_xs, ElementType::discriminator}});
    if (! hasEntered)
    {
        if (localname == "ordered"_xs)
        {
            auto value = valueAttributeFrom(attributes);
            if (value == "true")
            {
                mSlicingBuilder->ordered(true);
            }
            else if (value == "false")
            {
                mSlicingBuilder->ordered(false);
            }
            else
            {
                Fail2("Invalid value for slicing rule: " + value, std::logic_error);
            }
        }
        else if (localname == "rules"_xs)
        {
            auto value = valueAttributeFrom(attributes);
            auto rules = magic_enum::enum_cast<FhirSlicing::SlicingRules>(value);
            Expect(rules.has_value(), "Unknown dicriminator rule: " + value);
            mSlicingBuilder->slicingRules(*rules);
        }
    }
}

void FhirStructureDefinitionParser::handleDiscriminatorSubtree(const xmlChar* localname, const xmlChar* uri,
                                                               const SaxHandler::AttributeList& attributes)
{
    mStack.emplace_back(ElementType::Ignored, localname);
    ensureFhirNamespace(uri);
    if (localname == "type"_xs)
    {
        auto value = valueAttributeFrom(attributes);
        auto type = magic_enum::enum_cast<FhirSliceDiscriminator::DiscriminatorType>(value);
        Expect(type.has_value(), "Unknown discriminator type: " + value);
        mDiscriminatorBuilder.type(*type);
    }
    else if (localname == "path"_xs)
    {
        auto value = valueAttributeFrom(attributes);
        mDiscriminatorBuilder.path(std::move(value));
    }
}

void FhirStructureDefinitionParser::leaveDiscriminator()
{
    auto discriminator = mDiscriminatorBuilder.getAndReset();
    //     TVLOG(1) << "discriminator: " << magic_enum::enum_name(discriminator.type()) << ": " << discriminator.path();
    mSlicingBuilder->addDiscriminator(std::move(discriminator));
}

void FhirStructureDefinitionParser::enterValueSubtree(ElementType type, std::string_view prefix,
                                                      const xmlChar* localname, const AttributeList& attributes)
{
    std::string_view localnameView{XmlStringView{localname}};
    mSlicingBuilder.emplace();
    auto& stackTop = mStack.emplace_back(type, localname);
    stackTop.valueBuilder = std::make_unique<FhirValue::Builder>(std::string{localnameView.substr(prefix.size())});
    copyValueAttriutes(attributes);
}

void FhirStructureDefinitionParser::handleValueSubtree(const XmlStringView localname, const xmlChar* uri,
                                                       const SaxHandler::AttributeList& attributes)
{
    ensureFhirNamespace(uri);
    auto& prev = mStack.back();
    auto& stackTop = mStack.emplace_back(mStack.back().type, localname.xs_str());
    stackTop.valueBuilder = std::make_unique<FhirValue::Builder>(std::string{localname}, *prev.valueBuilder);
    copyValueAttriutes(attributes);
}

void FhirStructureDefinitionParser::copyValueAttriutes(const AttributeList& attributes)
{
    auto& stackTop = mStack.back();
    for (size_t i = 0, end = attributes.size(); i < end; ++i)
    {
        const auto& attr = attributes.get(i);
        stackTop.valueBuilder->addAttribute(std::string{attr.localname()}, std::string{attr.value()});
    }
}

void FhirStructureDefinitionParser::leaveValue()
{
    using namespace std::string_literals;
    auto& stackTop = mStack.back();
    if (stackTop.type == ElementType::fixed)
    {
        mElementBuilder.fixed(std::move(*stackTop.valueBuilder));
        return;
    }
    Expect3(stackTop.type == ElementType::pattern,
            "Unexpected element type in leaveValue: "s.append(magic_enum::enum_name(stackTop.type)), std::logic_error);
    mElementBuilder.pattern(std::move(*mStack.back().valueBuilder));
}

void fhirtools::FhirStructureDefinitionParser::handleCodeSystemSubtree(const xmlChar* localname, const xmlChar* uri,
                                                                       const SaxHandler::AttributeList& attributes)
{
    ensureFhirNamespace(uri);
    if (localname == "url"_xs)
    {
        mCodeSystemBuilder.url(valueAttributeFrom(attributes));
    }
    else if (localname == "name"_xs)
    {
        mCodeSystemBuilder.name(valueAttributeFrom(attributes));
    }
    else if (localname == "version"_xs)
    {
        mCodeSystemBuilder.version(valueAttributeFrom(attributes));
    }
    else if (localname == "caseSensitive"_xs)
    {
        mCodeSystemBuilder.caseSensitive(valueAttributeFrom(attributes));
    }
    else if (localname == "content"_xs)
    {
        mCodeSystemBuilder.contentType(valueAttributeFrom(attributes));
    }
    else if (localname == "supplements"_xs)
    {
        mCodeSystemBuilder.supplements(valueAttributeFrom(attributes));
    }
    else if (localname == "concept"_xs)
    {
        mStack.emplace_back(ElementType::Concept, localname);
        return;
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleConceptSubtree(const xmlChar* localname, const xmlChar*,
                                                                    const SaxHandler::AttributeList& attributes)
{
    if (localname == "code"_xs)
    {
        mCodeSystemBuilder.code(valueAttributeFrom(attributes));
    }
    if (localname == "concept"_xs)
    {
        mStack.emplace_back(ElementType::Concept, localname);
        return;
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::leaveConcept()
{
    mCodeSystemBuilder.popConcept();
}

void fhirtools::FhirStructureDefinitionParser::leaveCodeSystem()
{
    mCodeSystems.emplace_back(mCodeSystemBuilder.getAndReset());
}

void fhirtools::FhirStructureDefinitionParser::handleValueSetSubtree(const xmlChar* localname, const xmlChar* uri,
                                                                     const SaxHandler::AttributeList& attributes)
{
    ensureFhirNamespace(uri);
    if (localname == "url"_xs)
    {
        mValueSetBuilder.url(valueAttributeFrom(attributes));
    }
    else if (localname == "name"_xs)
    {
        mValueSetBuilder.name(valueAttributeFrom(attributes));
    }
    else if (localname == "version"_xs)
    {
        mValueSetBuilder.version(valueAttributeFrom(attributes));
    }
    else if (localname == "compose"_xs)
    {
        mStack.emplace_back(ElementType::compose, localname);
        return;
    }
    else if (localname == "expansion"_xs)
    {
        mStack.emplace_back(ElementType::expansion, localname);
        return;
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeSubtree(const xmlChar* localname, const xmlChar*,
                                                                    const SaxHandler::AttributeList&)
{
    if (localname == "include"_xs)
    {
        mValueSetBuilder.include();
        mStack.emplace_back(ElementType::composeInclude, localname);
        return;
    }
    else if (localname == "exclude"_xs)
    {
        mValueSetBuilder.exclude();
        mStack.emplace_back(ElementType::composeExclude, localname);
        return;
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeIncludeSubtree(const xmlChar* localname, const xmlChar*,
                                                                           const SaxHandler::AttributeList& attributes)
{
    if (localname == "system"_xs)
    {
        mValueSetBuilder.includeCodeSystem(valueAttributeFrom(attributes));
    }
    else if (localname == "concept"_xs)
    {
        mStack.emplace_back(ElementType::composeIncludeConcept, localname);
        return;
    }
    else if (localname == "filter"_xs)
    {
        mValueSetBuilder.includeFilter();
        mStack.emplace_back(ElementType::composeIncludeFilter, localname);
        return;
    }
    else if (localname == "valueSet"_xs)
    {
        mValueSetBuilder.includeValueSet(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeIncludeConceptSubtree(
    const xmlChar* localname, const xmlChar*, const SaxHandler::AttributeList& attributes)
{
    if (localname == "code"_xs)
    {
        mValueSetBuilder.includeCode(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeIncludeFilterSubtree(
    const xmlChar* localname, const xmlChar*, const SaxHandler::AttributeList& attributes)
{
    if (localname == "op"_xs)
    {
        mValueSetBuilder.includeFilterOp(valueAttributeFrom(attributes));
    }
    else if (localname == "value"_xs)
    {
        mValueSetBuilder.includeFilterValue(valueAttributeFrom(attributes));
    }
    else if (localname == "property"_xs)
    {
        mValueSetBuilder.includeFilterProperty(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeExcludeSubtree(const xmlChar* localname, const xmlChar*,
                                                                           const SaxHandler::AttributeList& attributes)
{
    if (localname == "system"_xs)
    {
        mValueSetBuilder.excludeCodeSystem(valueAttributeFrom(attributes));
    }
    else if (localname == "concept"_xs)
    {
        mStack.emplace_back(ElementType::composeExcludeConcept, localname);
        return;
    }
    else if (localname == "filter"_xs)
    {
        FPFail("Unsupported: ValueSet.compose.exclude.filter");
    }
    else if (localname == "valueSet"_xs)
    {
        mValueSetBuilder.includeValueSet(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleComposeExcludeConceptSubtree(
    const xmlChar* localname, const xmlChar*, const SaxHandler::AttributeList& attributes)
{
    if (localname == "code"_xs)
    {
        mValueSetBuilder.excludeCode(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleExpansionSubtree(const xmlChar* localname, const xmlChar*,
                                                                      const SaxHandler::AttributeList&)
{
    if (localname == "contains"_xs)
    {
        mValueSetBuilder.newExpand();
        mStack.emplace_back(ElementType::expansionContains, localname);
        return;
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::handleExpansionContainsSubtree(
    const xmlChar* localname, const xmlChar*, const SaxHandler::AttributeList& attributes)
{
    if (localname == "code"_xs)
    {
        mValueSetBuilder.expandCode(valueAttributeFrom(attributes));
    }
    else if (localname == "system"_xs)
    {
        mValueSetBuilder.expandSystem(valueAttributeFrom(attributes));
    }
    mStack.emplace_back(ElementType::Ignored, localname);
}

void fhirtools::FhirStructureDefinitionParser::leaveValueSet()
{
    mValueSets.emplace_back(mValueSetBuilder.getAndReset());
}

void fhirtools::FhirStructureDefinitionParser::handleBindingSubtree(const xmlChar* localname, const xmlChar*,
                                                                    const SaxHandler::AttributeList& attributes)
{
    mStack.emplace_back(ElementType::Ignored, localname);
    if (localname == "strength"_xs)
    {
        mElementBuilder.bindingStrength(valueAttributeFrom(attributes));
    }
    else if (localname == "valueSet"_xs)
    {
        mElementBuilder.bindingValueSet(valueAttributeFrom(attributes));
    }
}


std::string FhirStructureDefinitionParser::getPath()
{
    std::ostringstream result;
    for (const auto& info : mStack)
    {
        result << "/" << info.elementName;
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
    Expect3(uri == constants::namespaceUri,
            "unexpected xml uri '"s.append(reinterpret_cast<const char*>(uri)) + "' in " + getPath(), std::logic_error);
}
