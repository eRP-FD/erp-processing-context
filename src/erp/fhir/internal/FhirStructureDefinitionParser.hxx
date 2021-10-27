/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITIONPARSER_HXX
#define ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITIONPARSER_HXX

#include "erp/fhir/FhirStructureDefinition.hxx"
#include "erp/xml/SaxHandler.hxx"

#include <deque>
#include <filesystem>
#include <initializer_list>
#include <list>
#include <optional>
#include <tuple>

class XmlStringView;

/// @brief algorithmic class to parse FHIR-StructureDefinitions or Bundles thereof from XML-files
///
/// The internal state machine is controlled by the ElementType.
/// startElement pushes information on the stack, that stores information about the surrounding elements.
/// Some elements have dedicated functions starting with `handleXXXSubtree`, that will be called by startElement while the parser is in
/// that element. These dedicated functions extract the relevant information and use the Builders to construct
/// the FhirStructureDefinition and FhirElement instances.
/// Some elements have a function, that starts with `leave`, which is called by endElement once that element is left.
/// These functions join the collected data to construct the final list, which is returned to the caller.
class FhirStructureDefinitionParser final
    : private SaxHandler
{
public:

    ~FhirStructureDefinitionParser() override;

    static std::list<FhirStructureDefinition> parse(const std::filesystem::path& fileName);

private:
    enum class ElementType : uint8_t;
    class ElementInfo
    {
    public:
        ElementInfo(ElementType elementType, const xmlChar* elementName);
        ElementType type;
        std::string elementName;

    };


    FhirStructureDefinitionParser();

    // SaxParser callbacks:
    void startDocument() override;
    void endDocument() override;
    void startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                      int nbNamespaces, const xmlChar** namespaces, int nbDefaulted,
                      const SaxHandler::AttributeList& attributes) override;
    void endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri) override;


    // States and state transitions:

    bool enter(const xmlChar* localname, const xmlChar* uri,
               const std::initializer_list<std::tuple<const XmlStringView, ElementType>>& elements);

    void handleStructureDefinitionSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveStructureDefinition();

    void handleSnapshotSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    void handleElementSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);
    void leaveElement();

    void handleBaseSubTree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    void handleTypeSubtree(const xmlChar* localname, const xmlChar* uri, const AttributeList& attributes);

    // helper functions:
    std::string getPath();
    std::string valueAttributeFrom(const AttributeList& attributes);
    void ensureFhirNamespace(const xmlChar* uri);

    std::list<FhirStructureDefinition> mStructures;
    FhirStructureDefinition::Builder mStructureBuilder;
    FhirElement::Builder mElementBuilder;
    std::list<std::string> mElementTypes;
    bool mExperimental = false;
    std::deque<ElementInfo> mStack;
};

#endif// ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITIONPARSER_HXX
