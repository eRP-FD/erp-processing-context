/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTILS
#define ERP_PROCESSING_CONTEXT_TEST_UTILS

#include "erp/model/ResourceVersion.hxx"
#include "erp/validation/SchemaType.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace testutils
{

template<typename T>
static void waitFor(T predicate)
{
    for (size_t retries = 0; retries < 100; ++retries)
    {
        if (predicate())
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(predicate());
}

std::vector<EnvironmentVariableGuard> getNewFhirProfileEnvironment();
std::vector<EnvironmentVariableGuard> getOldFhirProfileEnvironment();
std::vector<EnvironmentVariableGuard> getOverlappingFhirProfileEnvironment();
std::vector<EnvironmentVariableGuard> getPatchedFhirProfileEnvironment();
using NewFhirProfileEnvironmentGuard  = GuardWithEnvFrom<getNewFhirProfileEnvironment>;
using OldFhirProfileEnvironmentGuard  = GuardWithEnvFrom<getOldFhirProfileEnvironment>;
using OverlappingFhirProfileEnvironmentGuard  = GuardWithEnvFrom<getOverlappingFhirProfileEnvironment>;

std::string profile(SchemaType,
                    model::ResourceVersion::FhirProfileBundleVersion = model::ResourceVersion::currentBundle());
std::string_view gkvKvid10(model::ResourceVersion::FhirProfileBundleVersion = model::ResourceVersion::currentBundle());
std::string_view
prescriptionIdNamespace(model::ResourceVersion::FhirProfileBundleVersion = model::ResourceVersion::currentBundle());
std::string_view
telematikIdNamespace(model::ResourceVersion::FhirProfileBundleVersion = model::ResourceVersion::currentBundle());

std::set<fhirtools::ValidationError> validationResultFilter(const fhirtools::ValidationResults& validationResult,
                                                            const fhirtools::ValidatorOptions& options);
} // namespace testutils

#endif// ERP_PROCESSING_CONTEXT_TEST_UTILS
