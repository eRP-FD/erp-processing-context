/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX

#include "erp/util/Version.hxx"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace fhirtools
{

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
        std::optional<std::string> codeSystemUrl;
        std::set<std::string> valueSets;
        // optionally include only a subset of the CodeSystem:
        std::set<std::string> codes;
        std::vector<Filter> filters;
    };
    struct Expansion {
        std::string codeSystemUrl;
        std::string code;
    };

    struct Code {
        std::string code;
        bool caseSensitive;
        std::string codeSystem;
        bool operator==(const Code&) const;
        std::strong_ordering operator<=>(const Code&) const;
    };

    [[nodiscard]] const std::string& getUrl() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getIncludes() const;
    [[nodiscard]] const std::vector<IncludeOrExclude>& getExcludes() const;
    [[nodiscard]] const std::vector<Expansion>& getExpands() const;
    [[nodiscard]] const Version& getVersion() const;

    [[nodiscard]] bool containsCode(const std::string& code) const;
    [[nodiscard]] bool containsCode(const std::string& code, const std::string& codeSystem) const;
    [[nodiscard]] const std::set<Code>& getCodes() const;
    [[nodiscard]] std::string codesToString() const;

    void finalize(class FhirStructureRepository* repo);
    [[nodiscard]] bool finalized() const;
    [[nodiscard]] bool canValidate() const;
    // Warnings may occur during loading and finalization. These Warnings are reported during each validation.
    [[nodiscard]] std::string getWarnings() const;


private:
    void addError(const std::string& error);
    void finalizeIncludeValueSets(FhirStructureRepository* repo, const std::set<std::string>& valueSets);
    void finalizeIncludeCodes(const std::set<std::string>& codes, bool caseSensitive, const std::string& codeSystemUrl);
    void finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                const std::string& codeSystemUrl, const class FhirCodeSystem* codeSystem,
                                bool caseSensitive);


    std::string mUrl;
    std::string mName;
    Version mVersion;
    std::vector<IncludeOrExclude> mIncludes;
    std::vector<IncludeOrExclude> mExcludes;
    std::vector<Expansion> mExpands;
    std::string mValidationWarning;
    bool mCanValidate = false;

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
    Builder& version(const std::string& version);
    Builder& include();
    Builder& includeCodeSystem(const std::string& system);
    Builder& includeValueSet(const std::string& valueSet);
    Builder& includeFilter();
    Builder& includeFilterOp(const std::string& filter);
    Builder& includeFilterValue(const std::string& value);
    Builder& includeFilterProperty(const std::string& value);
    Builder& includeCode(const std::string& code);
    Builder& exclude();
    Builder& excludeCodeSystem(const std::string& system);
    Builder& excludeCode(const std::string& code);
    Builder& newExpand();
    Builder& expandCode(const std::string& code);
    Builder& expandSystem(const std::string& system);

    FhirValueSet getAndReset();

    const std::string& getName() const;

private:
    std::unique_ptr<FhirValueSet> mFhirValueSet;
};

std::ostream& operator<<(std::ostream& ostream, const fhirtools::FhirValueSet::Code& code);
}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRVALUESET_HXX
