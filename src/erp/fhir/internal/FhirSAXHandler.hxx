#ifndef ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX

#include "erp/fhir/Fhir.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/xml/SaxHandler.hxx"
#include "erp/xml/XmlHelper.hxx"
#include "erp/xml/XmlMemory.hxx"

#include <cstdarg>
#include <list>
#include <memory>
#include <rapidjson/document.h>

class FhirElement;
class FhirStructureDefinition;
class FhirStructureRepository;


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
/// @note The current implementation doesn't support subelemets in primitive types (ERP-4397) and xhtml (ERP-4468)
class FhirSaxHandler final
    : private SaxHandler
{
public:
    static model::NumberAsStringParserDocument parseXMLintoJSON(
        const FhirStructureRepository& repo, const std::string_view& xmlDocument,
        XmlValidatorContext* schemaValidationContext);

private:
    class Context;
    explicit FhirSaxHandler(const FhirStructureRepository& repo);
    ~FhirSaxHandler() override;

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

    void pushArrayContext(const Context& parentItem, const FhirElement& parentElement,
                       const FhirElement& element, const XmlStringView& localname);

    void pushJsonField(const std::string_view& name, const AttributeList& attributes, const FhirStructureDefinition& type, const FhirElement& element);
    void pushObject(const std::string_view& name, const SaxHandler::AttributeList& attributes, const FhirStructureDefinition& type, const FhirElement& element);
    void pushString(const std::string_view& name, const std::string_view& value, const FhirStructureDefinition& type, const FhirElement& element);
    void pushBoolean(const std::string_view& name, const std::string_view& value, const FhirStructureDefinition& type, const FhirElement& element);
    void pushNumber(const std::string_view& name, const std::string_view& value, const FhirStructureDefinition& type, const FhirElement& element);


    std::tuple<const FhirStructureDefinition&, const FhirElement&>
    getTypeAndElement(const FhirStructureDefinition& baseType,
                     const FhirStructureDefinition* parentType,
                     const FhirElement& baseElement,
                     const std::string_view& name) const;

    const FhirStructureDefinition& systemTypeFor(const FhirStructureDefinition& primitiveType) const;

    static std::string makeElementId(const FhirElement& baseElement, const std::string_view& name);

    std::string_view valueAttributeFrom(const AttributeList& attributes, const std::string_view& elementName) const;


    // return type ensures a constructor for rapidjson::Value exists, if it exists its always rapidjson::Value
    template <typename T>
    auto asJsonValue(T&& value) -> decltype(rapidjson::Value{std::forward<T>(value), std::declval<rapidjson::Document::AllocatorType>()});

    rapidjson::Value asJsonValue(const std::string_view& value);

    rapidjson::Value asJsonValue(const xmlChar* value);

    std::string getPath() const;

    void startXHTMLElement(const XmlStringView localname,
                           int nbNamespaces,
                           const xmlChar ** namespaces,
                           const AttributeList& attributes);
    void endXHTMLElement(const xmlChar * localname, const xmlChar * prefix, const xmlChar * uri);

    model::NumberAsStringParserDocument mResult; ///< will contain the final result document one all Contexts have been joined
    std::list<Context> mStack; ///< holds the nested contexts back() is the current context
    const FhirStructureRepository& mStructureRepo;
    const FhirStructureDefinition* const mBackBoneType; ///< used to quickly compare with Backbone by using pointer comparison
    UniqueXmlDocumentPtr mCurrentXHTMLDoc;
    xmlNodePtr mCurrentXHTMLNode = nullptr;
};


#endif // ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_FHIRSAX_HXX

