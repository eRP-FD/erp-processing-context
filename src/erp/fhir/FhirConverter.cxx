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
    return FhirSaxHandler::parseXMLintoJSON(Fhir::instance().structureRepository(), xmlDocument, nullptr);
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
