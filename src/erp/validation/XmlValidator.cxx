/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"
#include "erp/util/TLog.hxx"
#include "erp/validation/XmlValidator.hxx"

#include <magic_enum.hpp>


std::tuple<SchemaType, XmlValidator::SchemaPtr> XmlValidator::loadSchema(const std::string& filePath) const
{
    TVLOG(2) << "loading XML schema " << filePath;

    std::filesystem::path schemaFilePath(filePath);
    Expect(FileHelper::exists(schemaFilePath), "Configured XML-schema does not exist: " + schemaFilePath.string());
    const auto schemaType = magic_enum::enum_cast<SchemaType>(schemaFilePath.filename().replace_extension().string());
    Expect(schemaType.has_value(), "Could not determine schema type from filename: " + filePath);

    std::unique_ptr<xmlSchemaParserCtxt, std::function<void(xmlSchemaParserCtxtPtr)>> xmlSchemaParserContext(
        ::xmlSchemaNewParserCtxt(schemaFilePath.string().c_str()),
        [](::xmlSchemaParserCtxtPtr ptr) { ::xmlSchemaFreeParserCtxt(ptr); });

    // stored exception, if any occurred in one of the handlers during parsing.
    std::exception_ptr exceptionPtr;

    ::xmlSchemaSetParserErrors(xmlSchemaParserContext.get(), XmlValidator::errorCallback, XmlValidator::warningCallback,
                               &exceptionPtr);
    ::xmlSchemaSetParserStructuredErrors(xmlSchemaParserContext.get(), XmlValidator::structuredErrorCallback,
                                         &exceptionPtr);

    auto schemaPtr = XmlValidator::SchemaPtr(::xmlSchemaParse(xmlSchemaParserContext.get()),
                                             [](::xmlSchemaPtr ptr) { ::xmlSchemaFree(ptr); });

    if (exceptionPtr)
    {
        std::rethrow_exception(exceptionPtr);
    }

    return std::make_tuple(*schemaType, std::move(schemaPtr));
}

void XmlValidator::loadSchemas(const std::vector<std::string>& paths)
{
    for (const auto& schemaFile : paths)
    {
        auto [schemaType, schemaPtr] = loadSchema(schemaFile);
        mMiscSchemaPtrs.try_emplace(schemaType, std::move(schemaPtr));
    }
}


void XmlValidator::loadKbvSchemas(const std::string& version, const std::vector<std::string>& paths,
                                  const std::optional<model::Timestamp>& validFrom,
                                  const std::optional<model::Timestamp>& validUntil)
{
    for (const auto& schemaFile : paths)
    {
        auto [schemaType, schemaPtr] = loadSchema(schemaFile);
        auto key = std::make_pair(schemaType, model::ResourceVersion::str_vKbv(version));
        mKbvXmlSchemaPtrs.try_emplace(key, std::make_tuple(std::move(schemaPtr), validFrom, validUntil));
    }
}


void XmlValidator::loadGematikSchemas(const std::string& version, const std::vector<std::string>& paths,
                                      const std::optional<model::Timestamp>& validFrom,
                                      const std::optional<model::Timestamp>& validUntil)
{
    for (const auto& schemaFile : paths)
    {
        auto [schemaType, schemaPtr] = loadSchema(schemaFile);
        auto key = std::make_pair(schemaType, model::ResourceVersion::str_vGematik(version));
        mGematikXmlSchemaPtrs.try_emplace(key, std::make_tuple(std::move(schemaPtr), validFrom, validUntil));
    }
}


std::unique_ptr<XmlValidatorContext>
XmlValidator::getSchemaValidationContext(SchemaType schemaType,
                                         model::ResourceVersion::DeGematikErezeptWorkflowR4 version) const
{
    const auto& candidate = mGematikXmlSchemaPtrs.find(std::make_pair(schemaType, version));
    const std::string schemaVersionHint =
        std::string(magic_enum::enum_name(schemaType)) + "|" + std::string(model::ResourceVersion::v_str(version));
    Expect(candidate != mGematikXmlSchemaPtrs.end(), "no schema loaded for " + schemaVersionHint);

    const auto& [schema, validFrom, validUntil] = candidate->second;
    Expect(schema != nullptr, "SchemaPtr is null");
    checkValidityTime(validFrom, validUntil, schemaVersionHint);

    return getContext(schema.get());
}

std::unique_ptr<XmlValidatorContext>
XmlValidator::getSchemaValidationContext(SchemaType schemaType, model::ResourceVersion::KbvItaErp version) const
{
    auto candidate = mKbvXmlSchemaPtrs.find(std::make_pair(schemaType, version));
    const std::string schemaVersionHint =
        std::string(magic_enum::enum_name(schemaType)) + "|" + std::string(model::ResourceVersion::v_str(version));
    Expect(candidate != mKbvXmlSchemaPtrs.end(), "no schema loaded for " + schemaVersionHint);
    const auto& [schema, validFrom, validUntil] = candidate->second;
    Expect(schema != nullptr, "SchemaPtr is null");
    checkValidityTime(validFrom, validUntil, schemaVersionHint);

    return getContext(schema.get());
}

std::unique_ptr<XmlValidatorContext>
XmlValidator::getSchemaValidationContext(SchemaType schemaType, model::ResourceVersion::NotProfiled) const
{
    return getSchemaValidationContextNoVer(schemaType);
}

std::unique_ptr<XmlValidatorContext> XmlValidator::getSchemaValidationContextNoVer(SchemaType schemaType) const
{
    auto candidate = mMiscSchemaPtrs.find(schemaType);
    Expect(candidate != mMiscSchemaPtrs.end() && candidate->second != nullptr,
           "no schema loaded for type " + std::string(magic_enum::enum_name(schemaType)));


    return getContext(candidate->second.get());
}

std::unique_ptr<XmlValidatorContext> XmlValidator::getContext(xmlSchemaPtr schemaPtr) const
{
    std::unique_ptr<XmlValidatorContext> xmlValidatorContext = std::make_unique<XmlValidatorContext>();

    xmlValidatorContext->mXmlSchemaValidCtxt = XmlValidatorContext::SchemaValidationContextPtr(
        ::xmlSchemaNewValidCtxt(schemaPtr), [](xmlSchemaValidCtxtPtr ptr) { ::xmlSchemaFreeValidCtxt(ptr); });
    Expect(xmlValidatorContext->mXmlSchemaValidCtxt, "Could not create schema validation context");

    ::xmlSchemaSetValidErrors(xmlValidatorContext->mXmlSchemaValidCtxt.get(), errorCallback, warningCallback,
                              &xmlValidatorContext->mExceptionPtr);
    ::xmlSchemaSetValidStructuredErrors(xmlValidatorContext->mXmlSchemaValidCtxt.get(), structuredErrorCallback,
                                        &xmlValidatorContext->mExceptionPtr);

    return xmlValidatorContext;
}

void XmlValidator::checkValidityTime(const std::optional<model::Timestamp>& validFrom,
                                     const std::optional<model::Timestamp>& validUntil,
                                     const std::string& schemaNameHint)
{
    const auto& now = model::Timestamp::now();
    if (validFrom.has_value())
    {
        ErpExpect(*validFrom <= now, HttpStatus::BadRequest,
                  "validity period of profile " + schemaNameHint + " not started. Will be valid from " +
                      validFrom->toXsDateTime());
    }
    if (validUntil.has_value())
    {
        ErpExpect(*validUntil >= now, HttpStatus::BadRequest,
                  "validity period of profile " + schemaNameHint + " is over. Was valid until " +
                      validFrom->toXsDateTime());
    }
}

void XmlValidator::errorCallback(void* context, const char* msg, ...)
{
    try
    {
        Expect3(msg != nullptr && context != nullptr, "callback called with nullptr", std::logic_error);
        if (context)
            TVLOG(1) << "in context of: " << *static_cast<std::string*>(context);

        std::va_list args{};
        va_start(args, msg);
        TVLOG(1) << "XML validation failed: " << String::vaListToString(msg, args);
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "XML validation failed", String::vaListToString(msg, args));
        va_end(args);
    }
    catch (...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
void XmlValidator::warningCallback(void* context, const char* msg, ...)
{
    try
    {
        Expect3(msg != nullptr && context != nullptr, "callback called with nullptr", std::logic_error);
        if (context)
            TVLOG(1) << "in context of: " << *static_cast<std::string*>(context);

        std::va_list args{};
        va_start(args, msg);
        TVLOG(1) << String::vaListToString(msg, args);
        va_end(args);
    }
    catch (...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
void XmlValidator::structuredErrorCallback(void* context, xmlErrorPtr error)
{
    try
    {
        Expect3(error != nullptr && context != nullptr, "callback called with nullptr", std::logic_error);

        switch (error->level)
        {
            case XML_ERR_NONE:
            case XML_ERR_WARNING:
                TVLOG(1) << error->message;
                break;
            case XML_ERR_ERROR:
            case XML_ERR_FATAL: {
                std::string_view message = error->message;
                if (message.back() == '\n')// remove trailing line feed contained in message from libxml2;
                    message = message.substr(0, message.size() - 1);
                TVLOG(1) << "On line " << error->line << ": " << message;
                ErpFailWithDiagnostics(HttpStatus::BadRequest,
                                       std::string("XML error on line ") + std::to_string(error->line),
                                       std::string(message));
            }
        }
    }
    catch (...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
