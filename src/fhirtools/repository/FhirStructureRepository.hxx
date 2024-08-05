/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"

#include <gsl/gsl-lite.hpp>
#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

namespace fhirtools
{
class FhirResourceGroupResolver;
class FhirStructureRepositoryFixer;
class FhirStructureRepositoryBackend;

class FhirStructureRepository : public std::enable_shared_from_this<FhirStructureRepository>
{
public:
    virtual ~FhirStructureRepository() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findStructure(const DefinitionKey& key) const = 0;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findTypeById(const std::string& typeName) const = 0;

    [[nodiscard]] virtual const FhirValueSet* findValueSet(const DefinitionKey& key) const = 0;

    [[nodiscard]] virtual const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const = 0;

    /// these are the system (builtin) types their type and url start with 'http://hl7.org/fhirpath/System.'
    virtual const FhirStructureDefinition* systemTypeBoolean() const = 0;
    virtual const FhirStructureDefinition* systemTypeString() const = 0;
    virtual const FhirStructureDefinition* systemTypeDate() const = 0;
    virtual const FhirStructureDefinition* systemTypeTime() const = 0;
    virtual const FhirStructureDefinition* systemTypeDateTime() const = 0;
    virtual const FhirStructureDefinition* systemTypeDecimal() const = 0;
    virtual const FhirStructureDefinition* systemTypeInteger() const = 0;

    struct ContentReferenceResolution {
        const FhirStructureDefinition& baseType;
        const FhirStructureDefinition& elementType;
        const size_t elementIndex;
        const std::shared_ptr<const FhirElement> element;
    };

    [[nodiscard]] virtual ContentReferenceResolution resolveContentReference(const FhirResourceGroup& group,
                                                                             const FhirElement& element) const = 0;
    virtual std::tuple<ProfiledElementTypeInfo, size_t>
    resolveBaseContentReference(std::string_view elementId) const = 0;
};

class DefaultFhirStructureRepositoryView : public FhirStructureRepository
{
public:
    explicit DefaultFhirStructureRepositoryView(std::string initId,
                                                gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    [[nodiscard]] std::string_view id() const override;
    [[nodiscard]] const FhirStructureDefinition* findStructure(const DefinitionKey& key) const override;
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const override;
    [[nodiscard]] const FhirValueSet* findValueSet(const DefinitionKey& url) const override;
    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const DefinitionKey& url) const override;

    const FhirStructureDefinition* systemTypeBoolean() const override;
    const FhirStructureDefinition* systemTypeString() const override;
    const FhirStructureDefinition* systemTypeDate() const override;
    const FhirStructureDefinition* systemTypeTime() const override;
    const FhirStructureDefinition* systemTypeDateTime() const override;
    const FhirStructureDefinition* systemTypeDecimal() const override;
    const FhirStructureDefinition* systemTypeInteger() const override;

    [[nodiscard]] ContentReferenceResolution resolveContentReference(const FhirResourceGroup& group,
                                                                     const FhirElement& element) const override;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const override;

private:
    std::string mId;
    gsl::not_null<const FhirStructureRepositoryBackend*> mBackend;
};

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

    [[nodiscard]] FhirStructureRepository::ContentReferenceResolution
    resolveContentReference(const FhirResourceGroup& group, const FhirElement& element) const;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const;

    FhirStructureRepositoryBackend(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend(FhirStructureRepositoryBackend&&) = delete;
    FhirStructureRepositoryBackend& operator=(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend& operator=(FhirStructureRepositoryBackend&&) = delete;

    std::shared_ptr<const fhirtools::DefaultFhirStructureRepositoryView>  defaultView() const;
    const std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>>& allGroups() const;

    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirStructureDefinition>>& definitionsByKey() const;
    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirCodeSystem>>& codeSystemsByKey() const;
    const std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>>& valueSetsByKey() const;

private:
    void loadFile(std::list<std::pair<FhirCodeSystem, std::filesystem::path>>& supplements,
                  const std::filesystem::path& file, const FhirResourceGroupResolver& groupResolver);
    void mergeGroups(const std::map<std::string, std::shared_ptr<const FhirResourceGroup>>& groups);
    void finalizeValueSets();
    void addSystemTypes();
    const FhirStructureDefinition* addSystemType(FhirStructureDefinition::Kind kind, const std::string_view& url);
    const FhirStructureDefinition* addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition);
    void addCodeSystem(std::unique_ptr<FhirCodeSystem>&& codeSystem);
    void addValueSet(std::unique_ptr<FhirValueSet>&& valueSet);
    void processCodeSystemSupplements(const std::list<std::pair<FhirCodeSystem, std::filesystem::path>>& supplements,
                                      const FhirResourceGroupResolver& groupResolver);

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

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>> mValueSetsByKey;

    std::shared_ptr<const DefaultFhirStructureRepositoryView> m_defaultFhirStructureRepositoryView;
    std::shared_ptr<const FhirResourceGroupResolver> mSystemGroupResolver;
    std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>> mAllGroups;

    friend class FhirStructureRepositoryFixer;
};
}

#endif// FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
