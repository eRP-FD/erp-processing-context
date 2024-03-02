/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_SAXPARSER_HXX
#define FHIR_TOOLS_SAXPARSER_HXX

#include "erp/validation/XmlValidator.hxx"

#include <filesystem>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <cstdarg>
#include <optional>
#include <string_view>

namespace fhirtools
{
class SaxHandler
{
public:

    SaxHandler();
    virtual ~SaxHandler();

    void validateStringView(const std::string_view& xmlDocument, const XmlValidatorContext& schemaValidationContext);

    /// @brief a single Attribute to get the related properties
    /// the libxml2-function callback gets a particulatly ugly structure for the attribute list.
    /// The classes Attribute and AttributeList are wrappers around this structure
    class Attribute
    {
    public:
        std::string_view localname() const;
        std::optional<std::string_view> prefix() const;
        std::optional<std::string_view> uri() const;
        std::string_view value() const;
    private:
        explicit Attribute(const char** const attributes)
            : mData(attributes)
        {}
        /// each attribute is described by 5 character pointers:
        /// localname: zero terminated c-string
        /// prefix: zero terminated c-string
        /// value-begin: pointer to the beginning of the value
        /// value-end: pointer to the past the end of the value attribute.
        /// this structure originates from the function parameter attributes in startElementNsSAX2Func
        /// see http://www.xmlsoft.org/html/libxml-parser.html#startElementNsSAX2Func
        const char** const mData;
        friend class SaxHandler;
    };

    /// @brief List of attributes as provided by startElement callback
    /// see SaxParser::Attribute for details
    class AttributeList
    {
    public:
        std::optional<Attribute> findAttribute(const std::string_view& localName) const;
        std::optional<Attribute> findAttribute(const std::string_view& localName, const std::string_view& namespaceURI) const;
        Attribute get(size_t index) const;
        size_t size() const {return mSize; }
    private:
        template <typename PredicateT>
        std::optional<Attribute> findAttibuteIf(const PredicateT& predicate) const;


        AttributeList(const xmlChar** const attributes, size_t count)
            : mAttributes(reinterpret_cast<const char**>(attributes))
            , mSize(count)
        {}
        /// see http://www.xmlsoft.org/html/libxml-parser.html#startElementNsSAX2Func
        /// this is a seuence of 5-tuples as described for Attribute::mData
        const char** const mAttributes;
        size_t mSize;
        friend class SaxHandler;
    };

    // SAX parse context
    std::unique_ptr<xmlParserCtxt, std::function<void(xmlParserCtxtPtr)>> mContext;

    // stored exception, if any occurred in one of the handlers during parsing.
    std::exception_ptr mExceptionPtr;

protected:

    void parseStringView(const std::string_view& xmlDocument);
    void parseAndValidateStringView(const std::string_view& xmlDocument, XmlValidatorContext& schemaValidationContext);
    void parseFile(const std::filesystem::path& fileName);

    virtual void startDocument() {}
    virtual void endDocument() {}
    virtual void characters(const xmlChar* ch, int len);

    // parameters are as in: http://www.xmlsoft.org/html/libxml-parser.html#startElementNsSAX2Func
    // only the attribute list (nb_attributes, attributes) is wrapped by AttributeList
    virtual void startElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                              int nbNamespaces, const xmlChar** namespaces,
                              int nbDefaulted, const AttributeList& attributes);

    virtual void endElement(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri);

    virtual void error(const std::string& msg);


private:
    void parseStringViewInternal(xmlSAXHandler& handler, const std::string_view& xmlDocument, void* userData);
    void startElementInternal(const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri,
                              int nbNamespaces, const xmlChar** namespaces, int nbAttributes,
                              int nbDefaulted, const xmlChar** attributes);
    static void cErrorCallback(void* self, const char* msg, ...);
    static void structuredErrorCallback(void* self, xmlErrorPtr err);
    xmlSAXHandler mHandler{};
};
}

#endif// FHIR_TOOLS_SAXPARSER_HXX
