#ifndef ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREREPOSITORY_HXX
#define ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREREPOSITORY_HXX

#include "FhirStructureDefinition.hxx"

#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>


/// @brief loads and manages FhirStructureDefinitions
class FhirStructureRepository
{
public:
    FhirStructureRepository();

    /// @brief loads the content of the files containing FHIR-FhirStructureDefinitions or Bundles thereof in XML format
    /// @note after the files are loaded a basic check is performed, all baseDefinitions and element types must be resolvable
    void load(const std::list<std::filesystem::path>& files);

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]]
    const FhirStructureDefinition* findDefinitionByUrl(const std::string& url) const;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]]
    const FhirStructureDefinition* findTypeById(const std::string& typeName) const;

    // clang-format off
    /// these are the system (builtin) types their type and url start with 'http://hl7.org/fhirpath/System.'
    const FhirStructureDefinition* systemTypeBoolean()  const { return mSystemTypeBoolean; }
    const FhirStructureDefinition* systemTypeString()   const { return mSystemTypeString; }
    const FhirStructureDefinition* systemTypeDate()     const { return mSystemTypeDate; }
    const FhirStructureDefinition* systemTypeTime()     const { return mSystemTypeTime; }
    const FhirStructureDefinition* systemTypeDateTime() const { return mSystemTypeDateTime; }
    const FhirStructureDefinition* systemTypeDecimal()  const { return mSystemTypeDecimal; }
    const FhirStructureDefinition* systemTypeInteger()  const { return mSystemTypeInteger; }
    // clang-format on

    struct ContentReferenceResolution
    {
        const FhirStructureDefinition& baseType;
        const FhirStructureDefinition& elementType;
        const size_t elementIndex;
        const FhirElement& element;
    };

    [[nodiscard]]
    ContentReferenceResolution resolveContentReference(const std::string_view& contentReference) const;

private:
    class Verifier;

    void addSystemTypes();
    const FhirStructureDefinition* addSystemType(FhirStructureDefinition::Kind kind, const std::string_view& url);
    const FhirStructureDefinition* addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition);
    void verify();

    const FhirStructureDefinition* mSystemTypeBoolean = nullptr;
    const FhirStructureDefinition* mSystemTypeString = nullptr;
    const FhirStructureDefinition* mSystemTypeDate = nullptr;
    const FhirStructureDefinition* mSystemTypeTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDateTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDecimal = nullptr;
    const FhirStructureDefinition* mSystemTypeInteger = nullptr;


    std::unordered_map<std::string, std::unique_ptr<FhirStructureDefinition>> mDefinitionsByUrl;
    std::unordered_map<std::string, FhirStructureDefinition*> mDefinitionsByTypeId;
};

#endif // ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREREPOSITORY_HXX
