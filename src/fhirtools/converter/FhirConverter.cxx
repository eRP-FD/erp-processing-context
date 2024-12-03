/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirConverter.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "fhirtools/converter/internal/FhirJsonToXmlConverter.hxx"
#include "fhirtools/converter/internal/FhirSAXHandler.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/util/Gsl.hxx"

//#include <algorithm>


model::NumberAsStringParserDocument FhirConverter::xmlStringToJson(const std::string_view& xmlDocument) const
{
    const auto repo = Fhir::instance().backend().defaultView();
    return FhirSaxHandler::parseXMLintoJSON(*repo, xmlDocument, nullptr);
}

UniqueXmlDocumentPtr FhirConverter::jsonToXml(const model::NumberAsStringParserDocument& jsonDOM) const
{
    const auto repo = Fhir::instance().backend().defaultView();
    return FhirJsonToXmlConverter::jsonToXml(*repo, jsonDOM);
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
