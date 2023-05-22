/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_FHIRCONVERTER_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_FHIRCONVERTER_HXX

#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/xml/XmlMemory.hxx"

#include <rapidjson/document.h>
#include <memory>
#include <string_view>

namespace fhirtools {
class FhirStructureRepository;
}
class XmlValidator;
enum class SchemaType;

namespace model
{
class NumberAsStringParserDocument;
}

/// @brief Converts FHIR structures from XML to JSON representation
/// For the conversion, is based on FHIR Structure Definitions taken from http://hl7.org/fhir/definitions.xml.zip
/// For details refer to the files extracted to ${CMAKE_BINARY_DIR}/resources/fhir/hl7.org
class FhirConverter final
{
public:
    model::NumberAsStringParserDocument xmlStringToJson(const std::string_view& xmlDocument) const;

    UniqueXmlDocumentPtr jsonToXml(const model::NumberAsStringParserDocument& jsonDOM) const;

    std::string jsonToXmlString(const model::NumberAsStringParserDocument& jsonDOM, bool formatted = false) const;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_FHIRCONVERTER_HXX
