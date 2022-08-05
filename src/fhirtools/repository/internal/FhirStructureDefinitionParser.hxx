/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef FHIR_PATH_FHIRSTRUCTUREDEFINITIONPARSER_HXX
#define FHIR_PATH_FHIRSTRUCTUREDEFINITIONPARSER_HXX

#include <deque>
#include <filesystem>
#include <initializer_list>
#include <list>
#include <optional>
#include <tuple>

#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/util/SaxHandler.hxx"


class XmlStringView;

namespace fhirtools
{

/// @brief algorithmic class to parse FHIR-StructureDefinitions or Bundles thereof from XML-files
///
/// The internal state machine is controlled by the ElementType.
/// startElement pushes information on the stack, that stores information about the surrounding elements.
/// Some elements have dedicated functions starting with `handleXXXSubtree`, that will be called by startElement while the parser is in
/// that element. These dedicated functions extract the relevant information and use the Builders to construct
/// the FhirStructureDefinition and FhirElement instances.
/// Some elements have a function, that starts with `leave`, which is called by endElement once that element is left.
/// These functions join the collected data to construct the final list, which is returned to the caller.
class FhirStructureDefinitionParser final : private SaxHandler
{
public:
    ~FhirStructureDefinitionParser() override;

    using ParseResult =
        std::tuple<std::list<FhirStructureDefinition>, std::list<FhirCodeSystem>, std::list<FhirValueSet>>;

    static ParseResult parse(const std::filesystem::path& fileName);

private:
    enum class ElementType : uint8_t;
    class ElementInfo;

    FhirStructureDefinitionParser();

    // SaxParser callbacks:
    void startDocument() override;
    void endDocument() override;
    void startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int nbNamespaces,
                      const xmlChar** namespaces, int nbDefaulted,
                      const SaxHandler::AttributeList& attributes) override;
    void endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri) override;


    // States and state transitions:

    bool enter(const xmlChar* localname, const xmlChar* uri,
               const std::initializer_list<std::tuple<const XmlStringView, ElementType>>& elements);

    void handleStructureDefinitionSubTree(const xmlChar* localname, const xmlChar* uri,
                                          const AttributeList& attributes);
    void leaveStructureDefinition();

    void handleSnapshotSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    void handleElementSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveElement();

    void handleBaseSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    void handleTypeSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    void handleConstraintSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveConstraint();
    void handleSlicingSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleDiscriminatorSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveDiscriminator();
    void enterValueSubtree(ElementType type, std::string_view prefix, const xmlChar* localname,
                           const AttributeList& attributes);
    void handleValueSubtree(const XmlStringView localname, const xmlChar* uri, const AttributeList& attributes);
    void copyValueAttriutes(const AttributeList&);
    void leaveValue();
    void handleCodeSystemSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleConceptSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveConcept();
    void leaveCodeSystem();
    void handleValueSetSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleComposeSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleComposeIncludeSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleComposeIncludeConceptSubtree(const xmlChar* localname, const xmlChar* uri,
                                            const AttributeList& attributes);
    void handleComposeIncludeFilterSubtree(const xmlChar* localname, const xmlChar* uri,
                                           const AttributeList& attributes);
    void handleComposeExcludeFilterSubtree(const xmlChar* localname, const xmlChar*,
                                           const AttributeList& attributes);
    void handleComposeExcludeSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleComposeExcludeConceptSubtree(const xmlChar* localname, const xmlChar* uri,
                                            const AttributeList& attributes);
    void handleExpansionSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void handleExpansionContainsSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveValueSet();
    void handleBindingSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    // helper functions:
    std::string getPath();
    std::string valueAttributeFrom(const AttributeList& attributes);
    void ensureFhirNamespace(const xmlChar* uri);

    std::list<FhirStructureDefinition> mStructures;
    std::list<FhirCodeSystem> mCodeSystems;
    std::list<FhirValueSet> mValueSets;
    FhirStructureDefinition::Builder mStructureBuilder;
    std::string mStructureType;
    FhirElement::Builder mElementBuilder;
    FhirConstraint::Builder mConstraintBuilder;
    std::optional<FhirSlicing::Builder> mSlicingBuilder;
    FhirSliceDiscriminator::Builder mDiscriminatorBuilder;
    FhirCodeSystem::Builder mCodeSystemBuilder;
    FhirValueSet::Builder mValueSetBuilder;
    std::list<std::string> mElementTypes;
    bool mExperimental = false;
    std::deque<ElementInfo> mStack;
    std::filesystem::path currentFile;
};
}
#endif// FHIR_PATH_FHIRSTRUCTUREDEFINITIONPARSER_HXX
