// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH
#pragma once

namespace fhirtools
{
enum class ExtendedValidation
{
    unslicedExtension,
    invalidUrnUuidInUri,
    bundleFullUrlMissing,
    bundleFullUrlIdMissmatch,
    bundleFullUrlResourceTypeMissmatch,
    bundleFullUrlInvalidFormat,
    bundledResourceMissingId,
    unresolveableReferenceInBundle,
};
}
