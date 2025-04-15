/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX

#include "shared/model/Resource.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/util/XmlHelper.hxx"
#include "fhirtools/util/XmlMemory.hxx"

#include <cstdarg>
#include <list>
#include <memory>
#include <rapidjson/document.h>

namespace fhirtools {
class FhirElement;
class FhirStructureDefinition;
class FhirStructureRepository;
}

/// @brief Algorithm class for parsing FHIR XML Documents into FHIR-JSON
///
/// The algorithm is based on a handler that receives events from a sax parser.
/// On each startElement eiter a new Context is created or the current Contexts depth value is incremented.
/// (a special case is the array, where an extra Context is created to hold the arrays content)
/// On each endElement the depth value is decremented and if it reaches zero, the value of that context is
/// integrated into its parent context.
/// When the last (root) element is closed, there is only one context on the stack, which will then hold
/// the result JSON-DOM, which will be set as root object for the result document (mResult)
/// for details about FHIR data representation refer to:
/// XML : http://hl7.org/fhir/R4/xml.html
/// JSON: http://hl7.org/fhir/R4/json.html
class FhirSaxHandler final
    : private fhirtools::SaxHandler
{
public:
    ~FhirSaxHandler() override;

    static model::NumberAsStringParserDocument parseXMLintoJSON(
        const fhirtools::FhirStructureRepository& repo, const std::string_view& xmlDocument,
        XmlValidatorContext* schemaValidationContext);

private:
    class Context;
    explicit FhirSaxHandler(const fhirtools::FhirStructureRepository& repo);

    model::NumberAsStringParserDocument parseXMLintoJSONInternal(
        const std::string_view& xmlDocument,
        XmlValidatorContext* schemaValidationContext);

    void endDocument() override;
    void characters(const xmlChar * ch, int len) override;
    void startElement(const xmlChar *localname,
                      const xmlChar *prefix,
                      const xmlChar *uri,
                      int nbNamespaces,
                      const xmlChar **namespaces,
                      int nbDefaulted,
                      const AttributeList& attributes) override;

    void endElement(const xmlChar * localname,
                    const xmlChar * prefix,
                    const xmlChar * uri) override;

    void startFHIRElement(const XmlStringView& localname, const AttributeList& attributes);

    void pushContextForSubObjectOfPrimitive(const std::string_view& localname);

    void joinTopStackElements();
    void joinContext(Context&& subValueContext);
    void joinSubObjectOfPrimitive(Context&& subObjectContext);


    // Push functions create a new context on the Parser stack
    void pushRootResource(const XmlStringView& resoureType);

    void pushArrayContext(const Context& parentItem, const std::shared_ptr<const fhirtools::FhirElement>& parentElement,
                       const std::shared_ptr<const fhirtools::FhirElement>& element, const XmlStringView& localname);

    void pushJsonField(const std::string_view& name, const AttributeList& attributes, const fhirtools::FhirStructureDefinition& type, std::shared_ptr<const fhirtools::FhirElement> element);
    void pushObject(const std::string_view& name, const SaxHandler::AttributeList& attributes, const fhirtools::FhirStructureDefinition& type, const std::shared_ptr<const fhirtools::FhirElement>& element);
    void pushString(const std::string_view& name, const std::string_view& value, const fhirtools::FhirStructureDefinition& type, std::shared_ptr<const fhirtools::FhirElement> element);
    void pushBoolean(const std::string_view& name, const std::string_view& value, const fhirtools::FhirStructureDefinition& type, std::shared_ptr<const fhirtools::FhirElement> element);
    void pushNumber(const std::string_view& name, const std::string_view& value, const fhirtools::FhirStructureDefinition& type, std::shared_ptr<const fhirtools::FhirElement> element);


    [[nodiscard]] std::tuple<const fhirtools::FhirStructureDefinition&, std::shared_ptr<const fhirtools::FhirElement>>
    getTypeAndElement(const fhirtools::FhirStructureDefinition& baseType,
                     const fhirtools::FhirStructureDefinition* parentType,
                     const fhirtools::FhirElement& baseElement,
                     const std::string_view& name) const;

    const fhirtools::FhirStructureDefinition& systemTypeFor(const fhirtools::FhirStructureDefinition& primitiveType) const;

    static std::string makeElementId(const fhirtools::FhirElement& baseElement, const std::string_view& name);

    // return type ensures a constructor for rapidjson::Value exists, if it exists its always rapidjson::Value
    template <typename T>
    auto asJsonValue(T&& value) -> decltype(rapidjson::Value{std::forward<T>(value), std::declval<rapidjson::Document::AllocatorType>()});

    rapidjson::Value asJsonValue(const std::string_view& value);

    rapidjson::Value asJsonValue(const xmlChar* value);

    static void dropTrailingNullsIfArray(rapidjson::Value& value);

    std::string getPath() const;

    void startXHTMLElement(const XmlStringView localname,
                           int nbNamespaces,
                           const xmlChar ** namespaces,
                           const AttributeList& attributes);
    void endXHTMLElement(const xmlChar * localname, const xmlChar * prefix, const xmlChar * uri);

    model::NumberAsStringParserDocument mResult; ///< will contain the final result document one all Contexts have been joined
    std::list<Context> mStack; ///< holds the nested contexts back() is the current context
    const fhirtools::FhirStructureRepository& mStructureRepo;
    UniqueXmlDocumentPtr mCurrentXHTMLDoc;
    xmlNodePtr mCurrentXHTMLNode = nullptr;
};


#endif // ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX
