/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "FhirConverter.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/internal/FhirJsonToXmlConverter.hxx"
#include "erp/fhir/internal/FhirSAXHandler.hxx"
#include "erp/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"

//#include <algorithm>


model::NumberAsStringParserDocument FhirConverter::xmlStringToJson(const std::string_view& xmlDocument) const
{
    return FhirSaxHandler::parseXMLintoJSON(Fhir::instance().structureRepository(), xmlDocument, {});
}

model::NumberAsStringParserDocument
FhirConverter::xmlStringToJsonWithValidationNoVer(const std::string_view& xmlDocument, const XmlValidator& validator,
                                                  SchemaType schemaType) const
{
    auto schemaValidationContext = validator.getSchemaValidationContextNoVer(schemaType);
    return FhirSaxHandler::parseXMLintoJSON(Fhir::instance().structureRepository(), xmlDocument,
                                            schemaValidationContext.get());
}

model::NumberAsStringParserDocument
FhirConverter::xmlStringToJsonWithValidation(const std::string_view& xmlDocument, const XmlValidator& validator,
                                             SchemaType schemaType,
                                             model::ResourceVersion::DeGematikErezeptWorkflowR4 schemaVersion) const
{
    auto schemaValidationContext = validator.getSchemaValidationContext(schemaType, schemaVersion);
    return FhirSaxHandler::parseXMLintoJSON(Fhir::instance().structureRepository(), xmlDocument,
                                            schemaValidationContext.get());
}
model::NumberAsStringParserDocument
FhirConverter::xmlStringToJsonWithValidation(const std::string_view& xmlDocument, const XmlValidator& validator,
                                             SchemaType schemaType,
                                             model::ResourceVersion::KbvItaErp schemaVersion) const
{
    auto schemaValidationContext = validator.getSchemaValidationContext(schemaType, schemaVersion);
    return FhirSaxHandler::parseXMLintoJSON(Fhir::instance().structureRepository(), xmlDocument,
                                            schemaValidationContext.get());
}

UniqueXmlDocumentPtr FhirConverter::jsonToXml(const model::NumberAsStringParserDocument& jsonDOM) const
{
    return FhirJsonToXmlConverter::jsonToXml(Fhir::instance().structureRepository(), jsonDOM);
}


std::string FhirConverter::jsonToXmlString(const model::NumberAsStringParserDocument& jsonDOM, bool formatted) const
{
    const auto& asXmlDoc = jsonToXml(jsonDOM);
    std::unique_ptr<xmlChar[], void (*)(void*)> buffer{nullptr, xmlFree};
    int size = 0;
    {
        xmlChar* rawBuffer = nullptr;
        xmlDocDumpFormatMemoryEnc(asXmlDoc.get(), &rawBuffer, &size, "utf-8", formatted ? 1 : 0);
        buffer.reset(rawBuffer);
        Expect(size > 0 && rawBuffer != nullptr, "XML document serialization failed.");
    }
    std::string result{reinterpret_cast<const char*>(buffer.get()), gsl::narrow<size_t>(size)};
    return result;
}
