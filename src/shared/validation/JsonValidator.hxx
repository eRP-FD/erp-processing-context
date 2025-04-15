/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX

#include "shared/validation/SchemaType.hxx"

#include <filesystem>
#include <rapidjson/fwd.h>
#include <unordered_map>
#include <vector>

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

private:
    // The schema for validating the schemas
    std::unique_ptr<rapidjson::SchemaDocument> mMetaSchema;

    // The fhir profile schemas
    std::unordered_map<SchemaType, std::unique_ptr<rapidjson::SchemaDocument>> mSchemas;
};


#endif//ERP_PROCESSING_CONTEXT_JSONVALIDATOR_HXX
