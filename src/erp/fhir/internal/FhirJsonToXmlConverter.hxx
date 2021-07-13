#ifndef ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX

#include "erp/xml/XmlMemory.hxx"
#include "rapidjson/fwd.h"

class FhirStructureRepository;
class FhirStructureDefinition;

namespace model
{
class NumberAsStringParserDocument;
}

/// @brief Convert Json-DOM to XML-DOM
///
/// The XML-Representation of a FHIR-Object requires that all memebers are in
/// the order of their definition in the StructureDefinition file.
/// Therefore instead of iterating over the members in the input, the algorithm
/// iterates over the elements in the structure definition and checks if the
/// member is contained in the input.
class FhirJsonToXmlConverter final
{
public:
    static UniqueXmlDocumentPtr jsonToXml(const FhirStructureRepository& repo,
                                          const model::NumberAsStringParserDocument& jsonDoc);


private:
    explicit FhirJsonToXmlConverter(const FhirStructureRepository& repo);

    void initRoot(UniqueXmlNodePtr rootNode);

    void convertResource(xmlNode* parentNode, const rapidjson::Value& resource);
    [[nodiscard]]
    size_t convertStructure(xmlNode& targetNode, const FhirStructureDefinition& type,
                          size_t fhirElementIndex, const rapidjson::Value& object);
    [[nodiscard]]
    size_t convertMember(xmlNode& targetNode, const std::string& memberName,
                         const FhirStructureDefinition& fhirParentType, size_t fhirElementIndex,
                         const rapidjson::Value& object, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertMemberWithType(xmlNode& targetNode, const std::string& memberName,
                         const FhirStructureDefinition& fhirParentType,
                         const FhirStructureDefinition& fhirElementType,
                         size_t fhirElementIndex,
                         const rapidjson::Value& object, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertSingleMember(xmlNode& targetNode, const std::string& memberName,
                       const FhirStructureDefinition& type, size_t fhirElementIndex,
                       const rapidjson::Value& object, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertXHTMLMember(xmlNode& targetNode, const std::string& memberName,
                                             const FhirStructureDefinition& fhirParentType, size_t fhirElementIndex,
                                             const rapidjson::Value& jsonMember);

    [[nodiscard]]
    static size_t skipElement(const FhirStructureDefinition& type, size_t elementIndex);

    static std::string valueString(const std::string_view& elementName, const rapidjson::Value& member);

    const FhirStructureRepository& mStructureRepository;

    UniqueXmlDocumentPtr mResultDoc;
    xmlNode* mRootNode = nullptr;
    xmlNs* mFhirNamespace = nullptr;
};

#endif// ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX
