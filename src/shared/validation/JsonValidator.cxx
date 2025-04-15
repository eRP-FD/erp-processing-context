/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "JsonValidator.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/TLog.hxx"
#include "shared/validation/RapidjsonErrorDocument.hxx"

#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/writer.h>
#include <iostream>


namespace
{

std::unordered_map<std::string, std::unique_ptr<rapidjson::SchemaDocument>> schemaCache;

std::unique_ptr<rapidjson::SchemaDocument> loadSchema(const std::filesystem::path& schemaFilePath,
                                                      rapidjson::SchemaDocument& metaSchema);

class ErpRemoteSchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider
{
public:
    explicit ErpRemoteSchemaDocumentProvider(std::filesystem::path basePath, rapidjson::SchemaDocument& metaSchema)
        : mBasePath(std::move(basePath))
        , mMetaSchema(metaSchema)
    {
    }

    const rapidjson::SchemaDocument* GetRemoteDocument(const char* remoteFile, rapidjson::SizeType size) override
    {
        auto filePath = std::filesystem::canonical(mBasePath / std::string_view(remoteFile, size));
        Expect3(std::filesystem::is_regular_file(filePath), "remote schema document not found: " + filePath.string(),
                std::logic_error);
        auto candidate = schemaCache.find(filePath.string());
        if (candidate != schemaCache.end())
        {
            return candidate->second.get();
        }
        auto [schema, emplaceResult] = schemaCache.try_emplace(filePath.string(), loadSchema(filePath, mMetaSchema));
        Expect(emplaceResult, "Could not insert into schema cache");
        return schema->second.get();
    }

    std::filesystem::path mBasePath;
    rapidjson::SchemaDocument& mMetaSchema;
};

template<typename TValue>
std::string docToString(const TValue& doc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}


void reportValidationError(const rapidjson::SchemaValidator& validator)
{
    TVLOG(1) << "validation failed for JSON document: " << docToString(validator.GetError());
    ErpFail(HttpStatus::BadRequest, "validation of JSON document failed");
}

std::unique_ptr<rapidjson::SchemaDocument> loadSchema(const std::filesystem::path& schemaFilePath,
                                                      rapidjson::SchemaDocument& metaSchema)
{
    const auto pathString = schemaFilePath.string();
    TVLOG(2) << "loading JSON schema " << pathString;
    Expect(FileHelper::exists(schemaFilePath), "Configured JSON-schema does not exist: " + pathString);
    const auto fileContent = FileHelper::readFileAsString(schemaFilePath);
    rapidjson::Document rjDocument;
    rjDocument.Parse(fileContent);
    Expect(! rjDocument.HasParseError(), "the schema is not a valid JSON: " + pathString);

    // validate the schema using the meta-schema
    rapidjson::SchemaValidator metaValidator(metaSchema);
    if (! rjDocument.Accept(metaValidator))
    {
        reportValidationError(metaValidator);
    }

    ErpRemoteSchemaDocumentProvider schemaDocumentProvider(schemaFilePath.parent_path(), metaSchema);
    return std::make_unique<rapidjson::SchemaDocument>(
        rjDocument, pathString.data(), static_cast<rapidjson::SizeType>(pathString.size()), &schemaDocumentProvider);
}


}

JsonValidator::JsonValidator() = default;
JsonValidator::~JsonValidator() = default;

void JsonValidator::loadSchema(const std::vector<std::string>& schemas, const std::filesystem::path& metaSchemaPath)
{
    {
        TVLOG(1) << "loading JSON meta-schema " << metaSchemaPath;
        const auto fileContent = FileHelper::readFileAsString(metaSchemaPath);
        rapidjson::Document rjMetaDocument;
        rjMetaDocument.Parse(fileContent);
        Expect(! rjMetaDocument.HasParseError(), "the meta-schema is not a valid JSON: " + metaSchemaPath.string());

        mMetaSchema = std::make_unique<rapidjson::SchemaDocument>(rjMetaDocument);
    }

    for (const auto& schemaPath : schemas)
    {
        std::filesystem::path schemaFilePath(schemaPath);
        const auto schemaType =
            magic_enum::enum_cast<SchemaType>(schemaFilePath.filename().replace_extension().string());
        Expect(schemaType.has_value(), "Could not determine schema type from filename: " + schemaPath);

        mSchemas.try_emplace(*schemaType, ::loadSchema(schemaPath, *mMetaSchema));
    }
}

void JsonValidator::validate(const rapidjson::Document& document, SchemaType schemaType) const
{
    auto candidate = mSchemas.find(schemaType);
    Expect(candidate != mSchemas.end(),
           "no JSON schema loaded for type " + std::string(magic_enum::enum_name(schemaType)));
    rapidjson::SchemaValidator validator(*candidate->second);
    validator.SetValidateFlags(rapidjson::kValidateDefaultFlags);
    if (! document.Accept(validator) || ! validator.IsValid())
    {
        RapidjsonErrorDocument errDoc(validator.GetError());
        std::string errStr;
        bool withDiag = false;
        if (errDoc.firstError())
        {
            errStr = docToString(*errDoc.firstError());
            withDiag = true;
        }
        else
        {
            errStr = docToString(validator.GetError());
        }
        TVLOG(1) << "The following document had validation errors: " << docToString(document);
        TVLOG(1) << "validation failed for JSON document: " << errStr;

        std::string details = "validation of JSON document failed";
        if (schemaType == SchemaType::fhir)
        {
            details.append(". FHIR JSON schema can be retrieved here: https://hl7.org/fhir/R4/fhir.schema.json.zip");
        }

        if (withDiag)
        {
            ErpFailWithDiagnostics(HttpStatus::BadRequest, details, errStr);
        }
        ErpFail(HttpStatus::BadRequest, details);
    }
}