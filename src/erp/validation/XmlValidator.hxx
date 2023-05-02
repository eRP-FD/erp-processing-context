/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX

#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/validation/SchemaType.hxx"

#include <libxml/xmlschemas.h>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct XmlValidatorContext {
    XmlValidatorContext() = default;
    XmlValidatorContext(const XmlValidatorContext&) = delete;
    using SchemaValidationContextPtr =
        std::unique_ptr<xmlSchemaValidCtxt, std::function<void(::xmlSchemaValidCtxtPtr)>>;
    SchemaValidationContextPtr mXmlSchemaValidCtxt;
    // stored exception, if any occurred in one of the handlers during parsing.
    std::exception_ptr mExceptionPtr;
};

class XmlValidator
{
public:
    using SchemaPtr = std::unique_ptr<xmlSchema, std::function<void(::xmlSchemaPtr)>>;

    void loadSchemas(const std::vector<std::string>& paths);
    void loadKbvSchemas(const std::string& version, const std::vector<std::string>& paths,
                        const std::optional<model::Timestamp>& validFrom,
                        const std::optional<model::Timestamp>& validUntil);
    void loadGematikSchemas(const std::string& version, const std::vector<std::string>& paths,
                            const std::optional<model::Timestamp>& validFrom,
                            const std::optional<model::Timestamp>& validUntil);

    std::unique_ptr<XmlValidatorContext>
    getSchemaValidationContext(SchemaType schemaType, model::ResourceVersion::DeGematikErezeptWorkflowR4 version) const;

    std::unique_ptr<XmlValidatorContext>
    getSchemaValidationContext(SchemaType schemaType,
                               model::ResourceVersion::DeGematikErezeptPatientenrechnungR4 version) const;
    std::unique_ptr<XmlValidatorContext>
    getSchemaValidationContext(SchemaType schemaType,
                               model::ResourceVersion::AbgabedatenPkv version) const;

    std::unique_ptr<XmlValidatorContext>
    getSchemaValidationContext(SchemaType schemaType,
                               model::ResourceVersion::WorkflowOrPatientenRechnungProfile version) const;

    std::unique_ptr<XmlValidatorContext>
    getSchemaValidationContext(SchemaType schemaType, model::ResourceVersion::Fhir version) const;

    std::unique_ptr<XmlValidatorContext> getSchemaValidationContext(SchemaType schemaType,
                                                                    model::ResourceVersion::KbvItaErp version) const;

    std::unique_ptr<XmlValidatorContext> getSchemaValidationContext(SchemaType schemaType,
                                                                    model::ResourceVersion::NotProfiled = {}) const;

    std::unique_ptr<XmlValidatorContext> getSchemaValidationContextNoVer(SchemaType schemaType) const;

    static void checkValidityTime(const std::optional<model::Timestamp>& validFrom,
                                  const std::optional<model::Timestamp>& validUntil, const std::string& schemaNameHint);

private:
    std::tuple<SchemaType, SchemaPtr> loadSchema(const std::string& filePath) const;
    std::unique_ptr<XmlValidatorContext> getContext(xmlSchemaPtr schemaPtr) const;

    using KeyGem = std::pair<SchemaType, model::ResourceVersion::DeGematikErezeptWorkflowR4>;
    using Value = std::tuple<SchemaPtr, std::optional<model::Timestamp>, std::optional<model::Timestamp>>;
    std::map<KeyGem, Value> mGematikXmlSchemaPtrs;

    using KeyKbv = std::pair<SchemaType, model::ResourceVersion::KbvItaErp>;
    std::map<KeyKbv, Value> mKbvXmlSchemaPtrs;

    std::map<SchemaType, SchemaPtr> mMiscSchemaPtrs;

    static void errorCallback(void*, const char* msg, ...);
    static void warningCallback(void*, const char* msg, ...);
    static void structuredErrorCallback(void* context, xmlErrorPtr error);
};


#endif//ERP_PROCESSING_CONTEXT_XMLVALIDATOR_HXX
