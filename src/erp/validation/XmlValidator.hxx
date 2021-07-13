#ifndef ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX

#include "erp/validation/SchemaType.hxx"

#include <libxml/xmlschemas.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct XmlValidatorContext
{
    XmlValidatorContext() = default;
    XmlValidatorContext(const XmlValidatorContext&) = delete;
    using SchemaValidationContextPtr = std::unique_ptr<xmlSchemaValidCtxt, std::function<void(::xmlSchemaValidCtxtPtr)>>;
    SchemaValidationContextPtr mXmlSchemaValidCtxt;
    // stored exception, if any occurred in one of the handlers during parsing.
    std::exception_ptr mExceptionPtr;
};

class XmlValidator
{
public:
    using SchemaPtr = std::unique_ptr<xmlSchema, std::function<void(::xmlSchemaPtr)>>;

    void loadSchemas(const std::vector<std::string>& paths);

    std::unique_ptr<XmlValidatorContext> getSchemaValidationContext(SchemaType schemaType) const;


private:
    std::unordered_map<SchemaType, SchemaPtr> mXmlSchemaPtrs;

    static void errorCallback(void*, const char* msg, ...);
    static void warningCallback(void*, const char *msg, ...);
    static void structuredErrorCallback(void *userData, xmlErrorPtr error);
};


#endif//ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX
