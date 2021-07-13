#include "erp/validation/XmlValidator.hxx"

#include <magic_enum.hpp>

#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/String.hxx"
#include "erp/util/TLog.hxx"

void XmlValidator::loadSchemas(const std::vector<std::string>& paths)
{
    for (const auto& schemaFile : paths)
    {
        TVLOG(2) << "loading XML schema " << schemaFile;

        std::filesystem::path schemaFilePath(schemaFile);
        Expect(FileHelper::exists(schemaFilePath), "Configured XML-schema does not exist: " + schemaFilePath.string());
        const auto schemaType = magic_enum::enum_cast<SchemaType>(schemaFilePath.filename().replace_extension().string());
        Expect(schemaType.has_value(), "Could not determine schema type from filename: " + schemaFile);

        std::unique_ptr<xmlSchemaParserCtxt, std::function<void(xmlSchemaParserCtxtPtr)>>
            xmlSchemaParserContext(
                ::xmlSchemaNewParserCtxt(schemaFilePath.string().c_str()),
                [](::xmlSchemaParserCtxtPtr ptr) { ::xmlSchemaFreeParserCtxt(ptr); });

        // stored exception, if any occurred in one of the handlers during parsing.
        std::exception_ptr exceptionPtr;

        ::xmlSchemaSetParserErrors(xmlSchemaParserContext.get(), errorCallback, warningCallback, &exceptionPtr);
        ::xmlSchemaSetParserStructuredErrors(xmlSchemaParserContext.get(), structuredErrorCallback, &exceptionPtr);

        mXmlSchemaPtrs.try_emplace(*schemaType,
                                   SchemaPtr(::xmlSchemaParse(xmlSchemaParserContext.get()),
                                                [](::xmlSchemaPtr ptr) { ::xmlSchemaFree(ptr); }));
        if (exceptionPtr)
        {
            std::rethrow_exception(exceptionPtr);
        }
    }
}

std::unique_ptr<XmlValidatorContext> XmlValidator::getSchemaValidationContext(SchemaType schemaType) const
{
    auto candidate = mXmlSchemaPtrs.find(schemaType);
    Expect(candidate != mXmlSchemaPtrs.end() && candidate->second != nullptr,
           "no schema loaded for type " + std::string(magic_enum::enum_name(schemaType)));

    std::unique_ptr<XmlValidatorContext> xmlValidatorContext = std::make_unique<XmlValidatorContext>();

    xmlValidatorContext->mXmlSchemaValidCtxt = XmlValidatorContext::SchemaValidationContextPtr(
        ::xmlSchemaNewValidCtxt(candidate->second.get()),
        [](xmlSchemaValidCtxtPtr ptr) { ::xmlSchemaFreeValidCtxt(ptr); });
    Expect(xmlValidatorContext->mXmlSchemaValidCtxt, "Could not create schema validation context");

    ::xmlSchemaSetValidErrors(xmlValidatorContext->mXmlSchemaValidCtxt.get(), errorCallback,
                              warningCallback, &xmlValidatorContext->mExceptionPtr);
    ::xmlSchemaSetValidStructuredErrors(xmlValidatorContext->mXmlSchemaValidCtxt.get(),
                                        structuredErrorCallback,
                                        &xmlValidatorContext->mExceptionPtr);

    return xmlValidatorContext;
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
        ErpFail(HttpStatus::BadRequest, "XML validation failed");
        va_end(args);
    } catch (...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
void XmlValidator::warningCallback(void* context, const char* msg, ...)
{
    try
    {
        Expect3(msg != nullptr && context != nullptr, "callback called with nullptr",
                std::logic_error);
        if (context)
            TVLOG(1) << "in context of: " << *static_cast<std::string*>(context);

        std::va_list args{};
        va_start(args, msg);
        TVLOG(1) << String::vaListToString(msg, args);
        va_end(args);
    } catch (...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
void XmlValidator::structuredErrorCallback(void* context, xmlErrorPtr error)
{
    try
    {
        Expect3(error != nullptr && context != nullptr, "callback called with nullptr",
                std::logic_error);

        switch (error->level)
        {
            case XML_ERR_NONE:
            case XML_ERR_WARNING:
                TVLOG(1) << error->message;
                break;
            case XML_ERR_ERROR:
            case XML_ERR_FATAL: {
                TVLOG(1) << "On line " << error->line << ": " << error->message;
                ErpFail(HttpStatus::BadRequest,
                        std::string("XML error on line ") + std::to_string(error->line));
            }
        }
    } catch(...)
    {
        *static_cast<std::exception_ptr*>(context) = std::current_exception();
    }
}
