// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_VALIDATOROPTIONS_H
#define FHIRTOOLS_VALIDATOROPTIONS_H

#include "fhirtools/validator/Severity.hxx"

#include <optional>

namespace fhirtools
{

/**
 * @brief Options to change features of the FhirPathValidator
 */
class ValidatorOptions
{
public:
    enum class ReportUnknownExtensionsMode {
        /// don't change slicing rules, closed stay closed, open stay open:
        disable,
        /// close all open extension slicings
        closeSlicing,
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
        /// A_27698 - non-Standard check for number of entries in meta.profile
        Severity missingOrExtraMetaProfile = Severity::debug;
        /// ERP-30000 - severity for invalid/non-lower-case UUID in field of type `uri` (or derived) - nullopt disables the check
        std::optional<Severity> invalidUrnUuidInUri = std::nullopt;
        /// severity when meta.profile contains a Profile that is unknown or not in view
        Severity unknownMetaProfile = Severity::error;
        /// C_12219
        std::optional<Severity> bundleFullUrlMissing = std::nullopt;
        std::optional<Severity> bundleFullUrlIdMissmatch = std::nullopt;
        std::optional<Severity> bundleFullUrlResourceTypeMissmatch = std::nullopt;
        std::optional<Severity> bundleFullUrlInvalidFormat = std::nullopt; ///< check format is uuid, FHIR-RESTful
        std::optional<Severity> bundledResourceMissingId = std::nullopt; ///< a Resource in a Bundle doesn't have an ID
        std::optional<Severity> unresolveableReferenceInBundle = std::nullopt; ///< a Resource in a Bundle doesn't have an ID

        Severity sliceDetection = Severity::debug;
        Severity resourceTypeDetection = Severity::debug;

    };
    SeverityLevels levels{};
    // collect additional information, useful for FHIR-Transformer
    bool collectInfo = false;
};

}

#endif// FHIRTOOLS_VALIDATOROPTIONS_H
