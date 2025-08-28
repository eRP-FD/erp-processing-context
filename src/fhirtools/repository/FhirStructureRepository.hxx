/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

#include <gsl/gsl-lite.hpp>
#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

namespace fhirtools
{
class FhirCodeSystem;
class FhirResourceGroupResolver;
class FhirStructureRepositoryFixer;
class FhirStructureRepositoryBackend;
class FhirValueSet;


/// @brief loads and manages FhirStructureDefinitions
class FhirStructureRepositoryBackend
{
public:
    FhirStructureRepositoryBackend();
    ~FhirStructureRepositoryBackend();

    /// @brief loads the content of the files containing FHIR-FhirStructureDefinitions or Bundles thereof in XML format
    /// @note after the files are loaded a basic check is performed, all baseDefinitions and element types must be resolvable
    void load(const std::list<std::filesystem::path>& filesAndDirectories,
              const FhirResourceGroupResolver& groupResolver);
    void synthesizeCodeSystem(const std::string& url, const FhirVersion& version,
                              const FhirResourceGroupResolver& groupResolver);
    void synthesizeValueSet(const std::string& url, const FhirVersion& version,
                            const FhirResourceGroupResolver& groupResolver);

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] const FhirStructureDefinition* findDefinition(std::string_view url, const FhirVersion& version) const;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const;

    [[nodiscard]] FhirValueSet* findValueSet(const std::string_view url, const FhirVersion& version);
    [[nodiscard]] const FhirValueSet* findValueSet(const std::string_view url, const FhirVersion& version) const;

    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const std::string_view url, const FhirVersion& version) const;
    [[nodiscard]] std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                    const FhirVersion& version) const;


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

    [[nodiscard]] FhirStructureRepositoryView::ContentReferenceResolution
    resolveContentReference(const FhirResourceGroup& group, const FhirElement& element) const;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const;

    FhirStructureRepositoryBackend(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend(FhirStructureRepositoryBackend&&) = delete;
    FhirStructureRepositoryBackend& operator=(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend& operator=(FhirStructureRepositoryBackend&&) = delete;

    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> defaultView() const;
    const std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>>& allGroups() const;

    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirStructureDefinition>>& definitionsByKey() const;
    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirCodeSystem>>& codeSystemsByKey() const;
    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>>& valueSetsByKey() const;

private:
    [[nodiscard]] bool loadFile(const std::filesystem::path& file, const FhirResourceGroupResolver& groupResolver);
    void mergeGroups(const std::map<std::string, std::shared_ptr<const FhirResourceGroup>>& groups);
    void addSystemTypes();
    const FhirStructureDefinition* addSystemType(FhirStructureDefinition::Kind kind, const std::string_view& url);
    const FhirStructureDefinition* addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition);
    void addCodeSystem(std::unique_ptr<FhirCodeSystem>&& codeSystem);
    void addValueSet(std::unique_ptr<FhirValueSet>&& valueSet);

    template<typename Self>
    static decltype(auto) findValueSetHelper(Self* self, const std::string_view url, const FhirVersion& version);

    std::tuple<ProfiledElementTypeInfo, size_t>
    resolveBaseContentReference(ProfiledElementTypeInfo&& pet, size_t index, size_t dot, std::string_view originalRef) const;

    const FhirStructureDefinition* mSystemTypeBoolean = nullptr;
    const FhirStructureDefinition* mSystemTypeString = nullptr;
    const FhirStructureDefinition* mSystemTypeDate = nullptr;
    const FhirStructureDefinition* mSystemTypeTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDateTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDecimal = nullptr;
    const FhirStructureDefinition* mSystemTypeInteger = nullptr;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirStructureDefinition>> mDefinitionsByKey;
    std::unordered_map<std::string, FhirStructureDefinition*> mDefinitionsByTypeId;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirCodeSystem>> mCodeSystemsByKey;
    std::unordered_multimap<std::string, const FhirCodeSystem*> mSupplementsByUrl;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>> mValueSetsByKey;

    std::shared_ptr<const FhirStructureRepositoryView> mDefaultFhirStructureRepositoryView;
    std::shared_ptr<const FhirResourceGroupResolver> mSystemGroupResolver;
    std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>> mAllGroups;

    friend class FhirStructureRepositoryFixer;
};
}

#endif// FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
