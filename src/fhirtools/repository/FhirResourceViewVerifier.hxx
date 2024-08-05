#ifndef FHIRTOOLS_FHIRRESOURCEVIEWVERIFIER_HXX
#define FHIRTOOLS_FHIRRESOURCEVIEWVERIFIER_HXX

#include "FhirResourceGroup.hxx"
#include "FhirStructureRepository.hxx"
#include "erp/util/TLog.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/ValueElement.hxx"

#include <boost/algorithm/string.hpp>
#include <stdexcept>

namespace fhirtools
{
class FhirResourceViewVerifier
{
public:
    explicit FhirResourceViewVerifier(const FhirStructureRepositoryBackend& repo,
                                      const FhirStructureRepository* repoView = nullptr);

    void verify();

private:
    void verifyStructure(const FhirStructureDefinition& def);

    void verifyElement(const FhirStructureDefinition& def, const FhirElement& element);

    void verifyElementProfiles(const FhirStructureDefinition& def, const FhirElement& element);

    void verifyProfileExists(const FhirResourceGroup& group, const std::string& profile, const std::string& context,
                             bool mustResolve);
    void verifyProfileExistsInGroup(const FhirResourceGroup& group, const std::string& profile,
                                    const std::string& context);
    void verifyProfileExistsInView(const std::string& profile, const std::string& context);


    void verifyBinding(const FhirElement& element, const std::shared_ptr<const FhirResourceGroup>& group);

    const FhirValueSet* tryResolve(const std::shared_ptr<const fhirtools::FhirResourceGroup>& group,
                                   const DefinitionKey& key, FhirElement::BindingStrength bindingStrength);

    void verifyContentReference(const FhirStructureDefinition& def, const FhirElement& element);

    void verifySlicing(const FhirStructureDefinition& def, const FhirElement& element);

    void parseConstraints(const FhirStructureDefinition& def, const FhirElement& element);

    void verifyFixedCodeSystems(const FhirElement& element, const std::string& context);

    void verifyFixedCodeSystems(const FhirElement& element, const ValueElement& fixedOrPattern,
                                const std::string& context);

    void verifyFixedCodeSystems(const std::string& codeSystemUrl, const std::string& context);

    void verifyValueSetsRequiredOnly();

    void verifyValueSet(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet);
    void verifyValueSetExcludes(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet);
    void verifyValueSetIncludes(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet);
    void verifyValueSetCodeSystem(const DefinitionKey& valueSetKey, const fhirtools::FhirCodeSystem* codeSystem,
                                  const std::set<std::string>& codes, const DefinitionKey& codeSystemKey);

    const FhirStructureRepositoryBackend& mRepo;
    const FhirStructureRepository* mView;
    bool mVerfied = true;
    FhirConstraint::ExpressionCache mExpressionCache;
    std::set<std::string> unresolvedBase;
    std::set<std::string> missingType;
    std::set<std::string> elementsWithUnresolvedType;
    std::set<std::string> unresolvedProfiles;
    std::set<std::string> unresolvedBindings;
    std::set<DefinitionKey> requiredValueSets;
    std::set<std::string> unresolvedCodeSystems;
    std::set<DefinitionKey> unresolvedValueSets;
};
}


#endif// FHIRTOOLS_FHIRRESOURCEVIEWVERIFIER_HXX
