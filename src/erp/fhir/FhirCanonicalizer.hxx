#ifndef ERP_PROCESSING_CONTEXT_FHIR_FHIRCANONICALIZER_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_FHIRCANONICALIZER_HXX

#include <rapidjson/document.h>

#include <map>
#include <string>
#include <sstream>

class FhirElement;
class FhirStructureDefinition;

class FhirCanonicalizer
{
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
        const FhirStructureDefinition* backboneStructDef,
        const FhirElement* backboneElement,
        const FhirStructureDefinition& objectStructDef,
        const std::string& objectName,
        const rapidjson::Value& object);
    static void serializeArray(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const FhirStructureDefinition* backboneStructDef,
        const FhirElement* backboneElement,
        const FhirStructureDefinition& objectStructDef,
        const std::string& arrayName,
        const rapidjson::Value& array);
    static void serializePrimitiveValue(
        std::ostringstream& buffer,
        const FhirStructureDefinition& elementStructDef,
        const std::string& name,
        const rapidjson::Value& value);

    static void serializeValue(
        size_t& immersionDepth,
        std::ostringstream& buffer,
        const FhirStructureDefinition* backboneStructDef,
        const FhirElement* backboneElement,
        const FhirStructureDefinition& elementStructDef,
        const std::string& name,
        const rapidjson::Value& value);

    static std::map<std::string, const rapidjson::Value*> getSortedMembers(
        size_t immersionDepth,
        const FhirStructureDefinition* backboneStructDef,
        const FhirElement* backboneElement,
        const FhirStructureDefinition& objectStructDef,
        const rapidjson::Value& object);

    static std::string buildElementPath(
        const FhirElement* backboneElement,
        const FhirStructureDefinition& objectStructDef,
        const std::string& objectName);

    static void removeDoubleWhitespaces(std::string& value);

    static bool elementHasToBeRemoved(
        size_t immersionDepth,
        const FhirStructureDefinition* backboneStructDef,
        const FhirElement* backboneElement,
        const FhirStructureDefinition& objectStructDef,
        const std::string& objectName);
    static bool doubleWhitespacesHaveToBeRemoved(const FhirStructureDefinition& elementStructDef);
};

#endif
