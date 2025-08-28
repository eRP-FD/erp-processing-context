/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <filesystem>
#include <memory>
#include <vector>

namespace fhirtools
{
struct DefinitionKey;
class FhirCodeSystemCodes;
class FhirResourceGroup;
class FhirResourceGroupResolver;
class FhirStructureRepositoryView;
/// @brief A FHIR CodeSystem. http://hl7.org/fhir/codesystem.html
/// CodeSystems are only relevant during parsing phase, and for finalization of the ValueSets.
/// During validation only ValueSets are relevant.
class FhirCodeSystem
{
public:
    class Builder;
    enum class ContentType
    {
        not_present,
        example,
        fragment,
        complete,
        supplement
    };
    struct Property {
        std::string code;
        std::string value;
    };
    struct Code {
        std::string code;
        // code is a sub-concept of isA:
        std::vector<std::string> isA;
        std::string parent;
        std::vector<Property> properties;
    };

    [[nodiscard]] const std::string& getUrl() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const FhirVersion& getVersion() const;
    [[nodiscard]] DefinitionKey key() const;
    [[nodiscard]] const std::filesystem::path& sourceFile() const;
    [[nodiscard]] std::shared_ptr<const FhirResourceGroup> resourceGroup() const;
    [[nodiscard]] bool isCaseSensitive() const;
    [[nodiscard]] FhirCodeSystemCodes getCodes(const FhirStructureRepositoryView& view) const;
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] ContentType getContentType() const;
    [[nodiscard]] const std::optional<DefinitionKey>& getSupplements() const;
    [[nodiscard]] bool isSynthesized() const;

private:
    std::string mUrl;
    std::string mName;
    FhirVersion mVersion = FhirVersion::notVersioned;
    std::filesystem::path mSourceFile;
    bool mCaseSensitive{false};
    std::vector<Code> mCodes;
    ContentType mContentType{ContentType::not_present};
    std::optional<DefinitionKey> mSupplements;
    bool mSynthesized{false};
    std::shared_ptr<const FhirResourceGroup> mGroup;
};

class FhirCodeSystemCodes : public std::vector<FhirCodeSystem::Code> {
public:
    FhirCodeSystemCodes(DefinitionKey key, std::vector<FhirCodeSystem::Code> codes, bool caseSensitive,
                        bool synthesized);
    const DefinitionKey& key() const;
    bool caseSensitive() const;
    bool synthesized() const;

    [[nodiscard]] bool containsCode(std::string_view code) const;
    [[nodiscard]] FhirCodeSystemCodes resolveIsA(const std::string& value, const std::string& property) const;
    [[nodiscard]] FhirCodeSystemCodes resolveIsNotA(const std::string& value, const std::string& property) const;
    [[nodiscard]] FhirCodeSystemCodes resolveEquals(const std::string& value, const std::string& property) const;

private:
    const FhirCodeSystem::Code& getCode(const std::string& code) const;
    static bool isA(const FhirCodeSystem::Code& code, const std::string& value);
    [[nodiscard]] FhirCodeSystemCodes resolveIsAConcept(const std::string& value) const;
    [[nodiscard]] FhirCodeSystemCodes resolveIsAParent(const std::string& value) const;

    DefinitionKey mKey;
    bool mCaseSensitive{false};
    bool mSynthesized;
};

class FhirCodeSystem::Builder
{
public:
    Builder();
    explicit Builder(const FhirCodeSystem& codeSystem);
    Builder& url(const std::string& url);
    Builder& name(const std::string& name);
    Builder& version(FhirVersion version);
    Builder& sourceFile(std::filesystem::path path);
    Builder& initGroup(const FhirResourceGroupResolver& resolver);
    Builder& caseSensitive(const std::string& caseSensitive);
    Builder& popConcept();
    Builder& code(const std::string& code);
    Builder& contentType(const std::string& content);
    Builder& supplements(const DefinitionKey& key);
    Builder& synthesized();
    Builder& addProperty();
    Builder& propertyCode(const std::string& code);
    Builder& propertyValue(const std::string& value);

    void reset();
    FhirCodeSystem getAndReset();

private:
    std::unique_ptr<FhirCodeSystem> mCodeSystem;
    std::vector<std::string> mConceptStack;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX
