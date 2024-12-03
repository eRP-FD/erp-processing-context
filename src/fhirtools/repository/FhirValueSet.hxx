/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace fhirtools
{

class FhirResourceGroup;
class FhirResourceGroupResolver;
class FhirStructureRepositoryBackend;

/// @brief A FHIR ValueSet. http://hl7.org/fhir/valueset.html
/// In parsing phase the ValueSets are built by the FhirValueSet::Builder.
/// When all structure definition files have been parsed all valuesets are finalized for validation.
class FhirValueSet
{
public:
    class Builder;
    enum class FilterOp
    {
        isA,
        isNotA,
        equals
    };
    struct Filter {
        FilterOp mOp{};
        std::string mValue;
        std::string mProperty;
    };
    struct IncludeOrExclude {
        std::optional<DefinitionKey> codeSystemKey;
        std::set<DefinitionKey> valueSets;
        // optionally include only a subset of the CodeSystem:
        std::set<std::string> codes;
        std::vector<Filter> filters;
    };
    struct Expansion {
        DefinitionKey codeSystemKey{"unset-expansion", std::nullopt};
        std::string code;
    };

    struct Code {
        std::string code;
        bool caseSensitive;
        std::string codeSystem;
        bool operator==(const Code&) const;
        std::strong_ordering operator<=>(const Code&) const;
    };

    [[nodiscard]] DefinitionKey key() const;
    [[nodiscard]] const std::string& getUrl() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getIncludes() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getExcludes() const;
    [[nodiscard]] const std::vector<Expansion>& getExpands() const;
    [[nodiscard]] const FhirVersion& getVersion() const;

    [[nodiscard]] bool containsCode(const std::string& code) const;
    [[nodiscard]] bool containsCode(const std::string& code, const std::string& codeSystem) const;
    [[nodiscard]] const std::set<Code>& getCodes() const;
    [[nodiscard]] std::string codesToString() const;
    [[nodiscard]] std::shared_ptr<const FhirResourceGroup> resourceGroup() const;

    void finalize(FhirStructureRepositoryBackend* repo);
    [[nodiscard]] bool finalized() const;
    [[nodiscard]] bool canValidate() const;
    [[nodiscard]] bool hasErrors() const;
    // Warnings may occur during loading and finalization. These Warnings are reported during each validation.
    [[nodiscard]] std::string getWarnings() const;

    void addError(const std::string& error);

private:
    void finalizeIncludes(FhirStructureRepositoryBackend* repo);
    void finalizeExcludes(FhirStructureRepositoryBackend* repo);
    void finalizeIncludeValueSets(FhirStructureRepositoryBackend* repo, const std::set<DefinitionKey>& valueSets);
    void finalizeIncludeCodes(const std::set<std::string>& codes, bool caseSensitive, const std::string& codeSystemUrl);
    void finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                const std::string& codeSystemUrl, const class FhirCodeSystem* codeSystem,
                                bool caseSensitive);


    std::string mUrl;
    std::string mName;
    FhirVersion mVersion = FhirVersion::notVersioned;
    std::vector<IncludeOrExclude> mIncludes;
    std::vector<IncludeOrExclude> mExcludes;
    std::vector<Expansion> mExpands;
    std::string mValidationWarning;
    bool mCanValidate = true;
    bool mHasErrors = false;
    bool mFinalized = false;
    std::shared_ptr<const FhirResourceGroup> mGroup;

    // after finalize():
    std::set<Code> mCodes;
};

/// used during parsing phase.
class FhirValueSet::Builder
{
public:
    Builder();

    Builder& url(const std::string& url);
    Builder& name(const std::string& name);
    Builder& version(FhirVersion version);
    Builder& initGroup(const FhirResourceGroupResolver& resolver, const std::filesystem::path& source);
    Builder& include();
    Builder& includeCodeSystem(DefinitionKey system);
    Builder& includeValueSet(DefinitionKey valueSet);
    Builder& includeFilter();
    Builder& includeFilterOp(const std::string& filter);
    Builder& includeFilterValue(const std::string& value);
    Builder& includeFilterProperty(const std::string& value);
    Builder& includeCode(const std::string& code);
    Builder& exclude();
    Builder& excludeCodeSystem(DefinitionKey system);
    Builder& excludeCode(const std::string& code);
    Builder& newExpand();
    Builder& expandCode(const std::string& code);
    Builder& expandSystem(DefinitionKey system);

    FhirValueSet getAndReset();

    const std::string& getName() const;

    DefinitionKey key();

private:
    std::unique_ptr<FhirValueSet> mFhirValueSet;
};

std::ostream& operator<<(std::ostream& ostream, const fhirtools::FhirValueSet::Code& code);
}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
