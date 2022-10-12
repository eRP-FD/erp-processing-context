// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

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
    /// detect Extensions, that are not defined for a given place in the Document and report with
    /// Severity::unclicedWarning
    bool reportUnknownExtensions = false;
    /// Allow non-literal references in Composition.author
    bool allowNonLiteralAuthorReference = false;
    class SeverityLevels {
    public:
        /// Severity level for bundled resouces, that are neither directly nor indirectly referenced from the Composition
        Severity unreferencedBundledResource = Severity::error;
        /// Severity level for contained resouces, that are neither directly nor indirectly referenced from their parent
        /// resouce
        Severity unreferencedContainedResource = Severity::error;
        /// Some references must be resolveable in a Bundle. This sets the Severity for failed resolution
        Severity mandatoryResolvableReferenceFailure = Severity::error;
    };
    SeverityLevels levels{};
};

}

#endif// FHIRTOOLS_VALIDATOROPTIONS_H
