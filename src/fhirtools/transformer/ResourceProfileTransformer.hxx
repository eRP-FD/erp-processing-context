/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_TRANSFORMATOR_RESOURCEPROFILETRANSFORMER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_TRANSFORMATOR_RESOURCEPROFILETRANSFORMER_HXX

#include "fhirtools/model/MutableElement.hxx"

#include <gsl/gsl-lite.hpp>
#include <regex>
#include <unordered_map>

namespace fhirtools
{

struct ValueMapping {
    std::string from;
    std::string to;
    std::optional<std::string>
        restrictFieldName{};// TODO: consider using full name "MedicationRequest.id" instead of only "id"
};

// @brief transform a given resource from profile A1 to profile A2, where A1 and A2 are both profiles of the same base A
//        data contained in A1 but not in A2 will be deleted. data not contained in A1 but in A2 will be
//        DataAbsentReason extension.
class ResourceProfileTransformer
{
public:
    struct Result {
        bool success{true};
        std::list<std::string> errors{};
        void merge(Result&& subResult);
        std::string summary() const;
    };
    struct Options {
        bool removeUnknownExtensionsFromOpenSlicing = true;
        std::set<std::string> keepExtensions;
    };

    explicit ResourceProfileTransformer(const FhirStructureRepository* repository,
                                        const FhirStructureDefinition* sourceProfile,
                                        const FhirStructureDefinition* targetProfile, const Options& options);
    ResourceProfileTransformer& map(const std::vector<ValueMapping>& valueMappings);
    ResourceProfileTransformer& map(const ValueMapping& valueMapping);

    Result applyMappings(const MutableElement& element) const;
    Result transform(MutableElement& element, const DefinitionKey& targetProfileDefinitionKey) const;
    void addDataAbsentExtension(const MutableElement& element, FhirStructureDefinition::Kind elementKind,
                                const std::string_view& elementFullPath,
                                const std::string_view& typeId,
                                const std::string_view& missingElementName) const;

private:
    void addDataAbsentExtension(const MutableElement& element,
                                const ValidationAdditionalInfo& validationAdditionalInfo) const;
    void removeElement(const MutableElement& element) const;

    gsl::not_null<const FhirStructureRepository*> mRepository;
    const FhirStructureDefinition* mSourceProfile;
    gsl::not_null<const FhirStructureDefinition*> mTargetProfile;
    std::vector<ValueMapping> mValueMappings;
    std::vector<ValueMapping> mFixedValues;
    std::shared_ptr<Element> mDataAbsentExtensionPrototype;
    Options mOptions;
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_TRANSFORMATOR_RESOURCEPROFILETRANSFORMER_HXX
