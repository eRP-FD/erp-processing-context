/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX

#include "erp/util/Version.hxx"

#include <memory>
#include <vector>

namespace fhirtools
{

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
    struct Code {
        std::string code;
        // code is a sub-concept of isA:
        std::vector<std::string> isA;
        std::string parent;
    };

    [[nodiscard]] const std::string& getUrl() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const Version& getVersion() const;
    [[nodiscard]] bool isCaseSensitive() const;
    [[nodiscard]] const std::vector<Code>& getCodes() const;
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] bool containsCode(const std::string_view& code) const;
    [[nodiscard]] ContentType getContentType() const;
    [[nodiscard]] const std::string& getSupplements() const;
    [[nodiscard]] std::vector<std::string> resolveIsA(const std::string& value, const std::string& property) const;
    [[nodiscard]] std::vector<std::string> resolveIsNotA(const std::string& value, const std::string& property) const;
    [[nodiscard]] std::vector<std::string> resolveEquals(const std::string& value, const std::string& property) const;
    [[nodiscard]] bool isSynthesized() const;

private:
    bool isA(const Code& code, const std::string& value) const;
    const Code& getCode(const std::string& code) const;
    std::vector<std::string> resolveIsAConcept(const std::string& value) const;
    std::vector<std::string> resolveIsAParent(const std::string& value) const;

    std::string mUrl;
    std::string mName;
    Version mVersion;
    bool mCaseSensitive{false};
    std::vector<Code> mCodes;
    ContentType mContentType{};
    std::string mSupplements;
    bool mSynthesized{false};
};

class FhirCodeSystem::Builder
{
public:
    Builder();
    explicit Builder(const FhirCodeSystem& codeSystem);
    Builder& url(const std::string& url);
    Builder& name(const std::string& name);
    Builder& version(const std::string& version);
    Builder& caseSensitive(const std::string& caseSensitive);
    Builder& popConcept();
    Builder& code(const std::string& code);
    Builder& contentType(const std::string& content);
    Builder& supplements(const std::string& canonical);
    Builder& synthesized();

    FhirCodeSystem getAndReset();

private:
    std::unique_ptr<FhirCodeSystem> mCodeSystem;
    std::vector<std::string> mConceptStack;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_REPOSITORY_FHIRCODESYSTEM_HXX
