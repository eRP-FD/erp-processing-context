/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREREPOSITORY_HXX

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
class FhirStructureRepositoryFixer;
class FhirStructureRepositoryBackend;

class FhirStructureRepository : public std::enable_shared_from_this<FhirStructureRepository>
{
public:
    virtual ~FhirStructureRepository() = default;

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findDefinitionByUrl(std::string_view url) const = 0;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findTypeById(const std::string& typeName) const = 0;

    [[nodiscard]] virtual const FhirValueSet* findValueSet(const std::string_view url,
                                                           std::optional<std::string> version) const = 0;

    [[nodiscard]] virtual const FhirCodeSystem* findCodeSystem(const std::string_view url,
                                                               std::optional<std::string> version) const = 0;

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

    [[nodiscard]] virtual ContentReferenceResolution resolveContentReference(const FhirElement& element) const = 0;
    virtual std::tuple<ProfiledElementTypeInfo, size_t>
    resolveBaseContentReference(std::string_view elementId) const = 0;
};

class DefaultFhirStructureRepositoryView : public FhirStructureRepository
{
public:
    explicit DefaultFhirStructureRepositoryView(gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    [[nodiscard]] const FhirStructureDefinition* findDefinitionByUrl(std::string_view url) const override;
    [[nodiscard]] const FhirStructureDefinition* findTypeById(const std::string& typeName) const override;
    [[nodiscard]] const FhirValueSet* findValueSet(const std::string_view url,
                                                   std::optional<std::string> version) const override;
    [[nodiscard]] const FhirCodeSystem* findCodeSystem(const std::string_view url,
                                                       std::optional<std::string> version) const override;

    const FhirStructureDefinition* systemTypeBoolean() const override;
    const FhirStructureDefinition* systemTypeString() const override;
    const FhirStructureDefinition* systemTypeDate() const override;
    const FhirStructureDefinition* systemTypeTime() const override;
    const FhirStructureDefinition* systemTypeDateTime() const override;
    const FhirStructureDefinition* systemTypeDecimal() const override;
    const FhirStructureDefinition* systemTypeInteger() const override;

    [[nodiscard]] ContentReferenceResolution resolveContentReference(const FhirElement& element) const override;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const override;

private:
    gsl::not_null<const FhirStructureRepositoryBackend*> mBackend;
};

struct DefinitionKey {
    explicit DefinitionKey(std::string_view urlWithVersion);
    DefinitionKey(std::string url, std::string version);
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

/// @brief loads and manages FhirStructureDefinitions
class FhirStructureRepositoryBackend
{
public:
    FhirStructureRepositoryBackend();
    ~FhirStructureRepositoryBackend();

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

    [[nodiscard]] FhirStructureRepository::ContentReferenceResolution
    resolveContentReference(const FhirElement& element) const;
    std::tuple<ProfiledElementTypeInfo, size_t> resolveBaseContentReference(std::string_view elementId) const;

    FhirStructureRepositoryBackend(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend(FhirStructureRepositoryBackend&&) = delete;
    FhirStructureRepositoryBackend& operator=(const FhirStructureRepositoryBackend&) = delete;
    FhirStructureRepositoryBackend& operator=(FhirStructureRepositoryBackend&&) = delete;

    std::shared_ptr<const fhirtools::DefaultFhirStructureRepositoryView>  defaultView() const;

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

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirStructureDefinition>, DefinitionKey::Hash> mDefinitionsByKey;
    std::unordered_map<std::string, FhirStructureDefinition*> mLatestDefinitionsByUrl;
    std::unordered_map<std::string, FhirStructureDefinition*> mDefinitionsByTypeId;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirCodeSystem>, DefinitionKey::Hash> mCodeSystemsByKey;
    std::unordered_map<std::string, FhirCodeSystem*> mLatestCodeSystemsByUrl;

    std::unordered_map<DefinitionKey, std::unique_ptr<FhirValueSet>, DefinitionKey::Hash> mValueSetsByKey;
    std::unordered_map<std::string, FhirValueSet*> mLatestValueSetsByUrl;

    std::shared_ptr<const fhirtools::DefaultFhirStructureRepositoryView> m_defaultFhirStructureRepositoryView;

    friend class FhirStructureRepositoryFixer;
};

template<typename Self>
decltype(auto) FhirStructureRepositoryBackend::findValueSetHelper(Self* self, const std::string_view url,
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
