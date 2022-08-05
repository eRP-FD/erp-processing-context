// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIRTOOLS_VALIDATOROPTIONS_H
#define FHIRTOOLS_VALIDATOROPTIONS_H

namespace fhirtools
{

/**
 * @brief Options to change features of the FhirPathValidator
 */
class ValidatorOptions
{
public:
    /// @brief detect Extensions, that are not defined for a given place in the Document and report with
    /// Severity::unclicedWarning
    bool reportUnknownExtensions = false;
};

}

#endif// FHIRTOOLS_VALIDATOROPTIONS_H
