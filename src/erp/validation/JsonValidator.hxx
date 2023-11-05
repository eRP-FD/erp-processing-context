/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX

#include <unordered_map>
#include <rapidjson/fwd.h>

#include "erp/util/Configuration.hxx"
#include "erp/validation/SchemaType.hxx"

namespace
{
class ErpRemoteSchemaDocumentProvider;
}

class JsonValidator
{
public:
    JsonValidator();
    ~JsonValidator();

    void loadSchema(const std::vector<std::string>& schemas, const std::filesystem::path& metaSchemaPath);

    /// @throws ErpException with status 400 if the document could not be validated against schema
    void validate(const rapidjson::Document& document, SchemaType schemaType) const;

    // Validates against the given JSON schema and returns the error details, if validation fails
    std::optional<std::string> validateWithErrorMessage(const rapidjson::Document& document, SchemaType schemaType) const;

private:
    // The schema for validating the schemas
    std::unique_ptr<rapidjson::SchemaDocument> mMetaSchema;

    // The fhir profile schemas
    std::unordered_map<SchemaType, std::unique_ptr<rapidjson::SchemaDocument>> mSchemas;
};


#endif//ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX
