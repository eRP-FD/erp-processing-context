/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX

#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"

namespace fhirtools
{
class FhirStructureRepositoryFixer;
/// @brief loads and manages FhirStructureDefinitions
class FhirStructureRepository
{
public:
    FhirStructureRepository();
    ~FhirStructureRepository();

    /// @brief loads the content of the files containing FHIR-FhirStructureDefinitions or Bundles thereof in XML format
    /// @note after the files are loaded a basic check is performed, all baseDefinitions and element types must be resolvable
    void load(const std::list<std::filesystem::path>& filesAndDirectories);

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] const FhirStructureDefinition* findDefinitionByUrl(std::string_view url) const;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const;

    [[nodiscard]] FhirValueSet* findValueSet(const std::string_view url, std::optional<std::string> version);
    [[nodiscard]] const FhirValueSet* findValueSet(const std::string_view url,
                                                   std::optional<std::string> version) const;

    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const std::string_view url,
                                                       std::optional<std::string> version) const;

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

    struct ContentReferenceResolution {
        const FhirStructureDefinition& baseType;
        const FhirStructureDefinition& elementType;
        const size_t elementIndex;
        const std::shared_ptr<const FhirElement> element;
    };

    [[nodiscard]] ContentReferenceResolution resolveContentReference(const FhirElement& element) const;
    std::tuple<ProfiledElementTypeInfo, size_t>
    resolveBaseContentReference(std::string_view elementId) const;

    FhirStructureRepository(const FhirStructureRepository&) = delete;
    FhirStructureRepository(FhirStructureRepository&&) = delete;
    FhirStructureRepository& operator = (const FhirStructureRepository&) = delete;
    FhirStructureRepository& operator = (FhirStructureRepository&&) = delete;
private:
    class Verifier;

    void loadFile(std::list<FhirCodeSystem>& supplements, const std::filesystem::path& file);
    void addSystemTypes();
    const FhirStructureDefinition* addSystemType(FhirStructureDefinition::Kind kind, const std::string_view& url);
    const FhirStructureDefinition* addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition);
    void addCodeSystem(std::unique_ptr<FhirCodeSystem>&& codeSystem);
    void addValueSet(std::unique_ptr<FhirValueSet>&& valueSet);
    void processCodeSystemSupplements(const std::list<FhirCodeSystem>& supplements);
    template<typename Self>
    static decltype(auto) findValueSetHelper(Self* self, const std::string_view url,
                                             std::optional<std::string> version);

    const FhirStructureDefinition* mSystemTypeBoolean = nullptr;
    const FhirStructureDefinition* mSystemTypeString = nullptr;
    const FhirStructureDefinition* mSystemTypeDate = nullptr;
    const FhirStructureDefinition* mSystemTypeTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDateTime = nullptr;
    const FhirStructureDefinition* mSystemTypeDecimal = nullptr;
    const FhirStructureDefinition* mSystemTypeInteger = nullptr;

    struct DefinitionKey {
        explicit DefinitionKey(std::string_view urlWithVersion);
        DefinitionKey(std::string url, std::string version);
        DefinitionKey(std::vector<std::string> splitUrlAndVersion);
        auto tie() const
        {
            return std::tie(url, version);
        }
        bool operator==(const DefinitionKey& other) const
        {
            return tie() == other.tie();
        }
        struct Hash {
            std::hash<std::string> h;
            size_t operator()(const DefinitionKey& key) const
            {
                return h(key.url) % h(key.version);
            }
        };

        std::string url;
        Version version;
    };


    std::unordered_map<DefinitionKey, std::unique_ptr<FhirStructureDefinition>, DefinitionKey::Hash> mDefinitionsByKey;
    std::unordered_map<std::string, FhirStructureDefinition*> mLatestDefinitionsByUrl;
    std::unordered_map<std::string, FhirStructureDefinition*> mDefinitionsByTypeId;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirCodeSystem>, DefinitionKey::Hash> mCodeSystemsByKey;
    std::unordered_map<std::string, FhirCodeSystem*> mLatestCodeSystemsByUrl;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>, DefinitionKey::Hash> mValueSetsByKey;
    std::unordered_map<std::string, FhirValueSet*> mLatestValueSetsByUrl;

    friend class FhirStructureRepositoryFixer;
};

template<typename Self>
decltype(auto) FhirStructureRepository::findValueSetHelper(Self* self, const std::string_view url,
                                                           std::optional<std::string> version)
{
    auto key = version.has_value() ? DefinitionKey{std::string{url}, *version} : DefinitionKey{url};
    if (key.version.empty())
    {
        auto candidate = self->mLatestValueSetsByUrl.find(key.url);
        if (candidate != self->mLatestValueSetsByUrl.end())
        {
            return candidate->second;
        }
    }
    else
    {
        auto candidate = self->mValueSetsByKey.find(key);
        if (candidate != self->mValueSetsByKey.end())
        {
            return candidate->second.get();
        }
    }
    return static_cast<FhirValueSet*>(nullptr);
}

}
#endif// FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
