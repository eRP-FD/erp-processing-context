/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX

#include "erp/xml/XmlMemory.hxx"
#include "rapidjson/fwd.h"

namespace fhirtools {
class FhirElement;
class FhirStructureRepository;
class FhirStructureDefinition;
}

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
    static UniqueXmlDocumentPtr jsonToXml(const fhirtools::FhirStructureRepository& repo,
                                          const model::NumberAsStringParserDocument& jsonDoc);


private:
    explicit FhirJsonToXmlConverter(const fhirtools::FhirStructureRepository& repo);

    void initRoot(UniqueXmlNodePtr rootNode);

    void convertResource(xmlNode* parentNode, const rapidjson::Value& resource);
    [[nodiscard]]
    size_t convertStructure(xmlNode& targetNode, const fhirtools::FhirStructureDefinition& type,
                          size_t fhirElementIndex, const rapidjson::Value& object);
    [[nodiscard]]
    size_t convertMember(xmlNode& targetNode, const std::string& memberName,
                         const fhirtools::FhirStructureDefinition& fhirParentType, size_t fhirElementIndex,
                         const rapidjson::Value* jsonMember, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertMemberWithType(xmlNode& targetNode, const std::string& memberName,
                         const fhirtools::FhirStructureDefinition& fhirParentType,
                         const fhirtools::FhirStructureDefinition& fhirElementType,
                         const std::shared_ptr<const fhirtools::FhirElement>& fhirElementInfo,
                         size_t fhirElementIndex,
                         const rapidjson::Value* jsonMember, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertSingleMember(xmlNode& targetNode, const std::string& memberName,
                       const fhirtools::FhirStructureDefinition& fhirType, size_t fhirElementIndex,
                       const rapidjson::Value* object, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertPrimitive(xmlNode& targetNode, const std::string& memberName,
                            const fhirtools::FhirStructureDefinition& fhirType, size_t fhirElementIndex,
                            const fhirtools::FhirStructureDefinition& fhirElementType,
                            const rapidjson::Value* jsonObject, const rapidjson::Value& subObjectOfPrimary);

    [[nodiscard]]
    size_t convertXHTMLMember(xmlNode& targetNode, const std::string& memberName,
                              const fhirtools::FhirStructureDefinition& fhirParentType, size_t fhirElementIndex,
                              const rapidjson::Value& jsonMember);

    [[nodiscard]]
    static size_t skipElement(const fhirtools::FhirStructureDefinition& type, size_t elementIndex);

    static std::string valueString(const std::string_view& elementName, const rapidjson::Value& member);

    static bool hasIndex(const rapidjson::Value* value, size_t idx);

    static rapidjson::SizeType elementCount(const rapidjson::Value* arr1);
    static rapidjson::SizeType maxElements(const rapidjson::Value* arr1, const rapidjson::Value* arr2);

    const fhirtools::FhirStructureRepository& mStructureRepository;

    UniqueXmlDocumentPtr mResultDoc;
    xmlNode* mRootNode = nullptr;
    xmlNs* mFhirNamespace = nullptr;
};

#endif// ERP_PROCESSING_CONTEXT_FHIRJSONHANDLER_HXX
