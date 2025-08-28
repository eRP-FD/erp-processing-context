/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "fhirtools/util/Gsl.hxx"

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
class FhirStructureRepositoryView;
class FhirCodeSystemCodes;

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
        FhirCodeSystemCodes filter(const FhirCodeSystemCodes& codeSystem) const;
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


    [[nodiscard]] DefinitionKey key() const;
    [[nodiscard]] const std::string& getUrl() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const std::filesystem::path& sourceFile() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getIncludes() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getExcludes() const;
    [[nodiscard]] const std::vector<Expansion>& getExpands() const;
    [[nodiscard]] const FhirVersion& getVersion() const;

    [[nodiscard]] std::shared_ptr<const FhirResourceGroup> resourceGroup() const;

    [[nodiscard]] bool canValidate() const;
    [[nodiscard]] bool hasErrors() const;
    // Warnings may occur during loading and finalization. These Warnings are reported during each validation.
    [[nodiscard]] std::string getWarnings() const;

    void addError(const std::string& error);

private:



    std::string mUrl;
    std::string mName;
    FhirVersion mVersion = FhirVersion::notVersioned;
    std::filesystem::path mSourceFile;
    std::vector<IncludeOrExclude> mIncludes;
    std::vector<IncludeOrExclude> mExcludes;
    std::vector<Expansion> mExpands;
    std::string mValidationWarning;
    bool mCanValidate = true;
    bool mHasErrors = false;
    std::shared_ptr<const FhirResourceGroup> mGroup;

    // after finalize():
};

class FhirValueSetCodes
    : public std::enable_shared_from_this<FhirValueSetCodes>
{
public:
    static std::shared_ptr<const FhirValueSetCodes> create(gsl::not_null<const FhirStructureRepositoryView*> view,
                                                           gsl::not_null<const FhirValueSet*> valueSet);
    struct Code {
        std::string code;
        bool caseSensitive;
        std::string codeSystem;
        bool operator==(const Code&) const;
        std::strong_ordering operator<=>(const Code&) const;
    };

    [[nodiscard]] DefinitionKey key() const;
    [[nodiscard]] bool containsCode(const std::string& code) const;
    [[nodiscard]] bool containsCode(const std::string& code, const std::string& codeSystem) const;
    [[nodiscard]] const std::set<Code>& getCodes() const;
    [[nodiscard]] std::string codesToString() const;
    [[nodiscard]] const FhirValueSet& valueSet() const;

    [[nodiscard]] bool hasErrors() const;
    // Warnings may occur during loading and finalization. These Warnings are reported during each validation.
    [[nodiscard]] std::string getWarnings() const;
    [[nodiscard]] bool canValidate() const;


private:
    FhirValueSetCodes(gsl::not_null<const FhirStructureRepositoryView*> view,
                      gsl::not_null<const FhirValueSet*> valueSet);
    void addError(const std::string& error);
    void finalize(const FhirStructureRepositoryView* repo);
    void finalizeIncludes(const FhirStructureRepositoryView* repo);
    void finalizeExcludes(const FhirStructureRepositoryView* repo);
    void finalizeIncludeValueSets(const FhirStructureRepositoryView* repo, const std::set<DefinitionKey>& valueSets);
    void finalizeIncludeCodes(const std::set<std::string>& codes, bool caseSensitive, const std::string& codeSystemUrl);
    void finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                const FhirCodeSystemCodes* codeSystem);
    void finalizeExcludeCodes(const std::set<std::string>& codes, bool caseSensitive, const std::string& codeSystemUrl);
    void finalizeExcludeFilters(const std::vector<FhirValueSet::Filter>& excludeFilters,
                                const FhirCodeSystemCodes* codeSystem);

    const FhirValueSet* mValueSet;
    std::string mValidationWarning;
    bool mCanValidate = true;
    bool mHasErrors = false;
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
    Builder& sourceFile(std::filesystem::path path);
    Builder& initGroup(const FhirResourceGroupResolver& resolver);
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
    Builder& excludeFilter();
    Builder& excludeFilterOp(const std::string& filter);
    Builder& excludeFilterValue(const std::string& value);
    Builder& excludeFilterProperty(const std::string& value);
    Builder& excludeCode(const std::string& code);
    Builder& newExpand();
    Builder& expandCode(const std::string& code);
    Builder& expandSystem(DefinitionKey system);

    void reset();
    FhirValueSet getAndReset();

    const std::string& getName() const;

    DefinitionKey key();

private:
    std::unique_ptr<FhirValueSet> mFhirValueSet;
};

std::ostream& operator<<(std::ostream& ostream, const fhirtools::FhirValueSetCodes::Code& code);
}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
