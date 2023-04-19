/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_FHIRCANONICALIZER_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_FHIRCANONICALIZER_HXX

#include "erp/ErpRequirements.hxx"

#include <rapidjson/document.h>

#include <map>
#include <string>
#include <sstream>

namespace fhirtools {
class FhirElement;
class FhirStructureDefinition;
}

class FhirCanonicalizer
{
    static constexpr auto implements = A_19029_05.start(
    "das Bundle entsprechend der Kanonisierungsregeln http://hl7.org/fhir/canonicalization/json#static normalisieren")
    && A_19029_05.finish();
public:
    /**
     * Serializes the FHIR resources into a canonical json string according to "http://hl7.org/fhir/json.html#canonical".
     * - No whitespace other than single spaces in property values and in the xhtml in the Narrative.
     * - Order properties alphabetically.
     * - The narrative (Resource.text) is omitted.
     * - In addition to narrative (Resource.text), the Resource.meta element is removed.
     */
    [[nodiscard]] static std::string serialize(const rapidjson::Value& resource);

protected:
    static void serializeResource(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const std::string& name,
        const rapidjson::Value& object);

    static void serializeObject(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const fhirtools::FhirStructureDefinition* backboneStructDef,
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& objectStructDef,
        const std::string& objectName,
        const rapidjson::Value& object);
    static void serializeArray(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const fhirtools::FhirStructureDefinition* backboneStructDef,
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& objectStructDef,
        const std::string& arrayName,
        const rapidjson::Value& array);
    static void serializePrimitiveValue(
        std::ostringstream& buffer,
        const fhirtools::FhirStructureDefinition& elementStructDef,
        const std::string& name,
        const rapidjson::Value& value);

    static void serializeValue(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const fhirtools::FhirStructureDefinition* backboneStructDef,
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& elementStructDef,
        const std::string& name,
        const rapidjson::Value& value);

    static std::map<std::string, const rapidjson::Value*> getSortedMembers(
        size_t immersionDepth,
        const fhirtools::FhirStructureDefinition* backboneStructDef,
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& objectStructDef,
        const rapidjson::Value& object);

    static std::string buildElementPath(
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& objectStructDef,
        const std::string& objectName);

    static void removeDoubleWhitespaces(std::string& value);

    static bool elementHasToBeRemoved(
        size_t immersionDepth,
        const fhirtools::FhirStructureDefinition* backboneStructDef,
        const std::shared_ptr<const fhirtools::FhirElement>& backboneElement,
        const fhirtools::FhirStructureDefinition& objectStructDef,
        const std::string& objectName);
    static bool doubleWhitespacesHaveToBeRemoved(const fhirtools::FhirStructureDefinition& elementStructDef);
};

#endif
