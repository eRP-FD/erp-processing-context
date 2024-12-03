// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_VALIDATOROPTIONS_H
#define FHIRTOOLS_VALIDATOROPTIONS_H

#include "fhirtools/validator/Severity.hxx"

namespace fhirtools
{

/**
 * @brief Options to change features of the FhirPathValidator
 */
class ValidatorOptions
{
public:
    enum class ReportUnknownExtensionsMode {
        /// don't report:
        disable,
        /// always report as unslicedWarning - this will report unknown-Extensions as unslicedWarning
        /// even if the slicing is closed, which would normally be an error.
        /// effectively making validation less strict for closed slicings
        enable,
        /// enable unslicedWarning only for Extensions that are open - effectively making validation more strict
        onlyOpenSlicing,
    };
    /// detect Extensions, that are not defined for a given place in the Document and report with
    /// Severity::unslicedWarning
    ReportUnknownExtensionsMode  reportUnknownExtensions = ReportUnknownExtensionsMode::disable;
    /// Allow non-literal references in Composition.author
    bool allowNonLiteralAuthorReference = false;
    /// run reference validation pass
    bool validateReferences = true;
    /// also validate against Profiles in meta.profle of the input document
    bool validateMetaProfiles = true;
    class SeverityLevels {
    public:
        /// Severity level for bundled resources, that are neither directly nor indirectly referenced from the Composition
        Severity unreferencedBundledResource = Severity::error;
        /// Severity level for contained resources, that are neither directly nor indirectly referenced from their parent
        /// resource
        Severity unreferencedContainedResource = Severity::error;
        /// Some references must be resolvable in a Bundle. This sets the Severity for failed resolution
        Severity mandatoryResolvableReferenceFailure = Severity::error;
    };
    SeverityLevels levels{};
    // collect additional information, useful for FHIR-Transformer
    bool collectInfo = false;
};

}

#endif// FHIRTOOLS_VALIDATOROPTIONS_H
