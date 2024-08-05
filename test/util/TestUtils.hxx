/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTILS
#define ERP_PROCESSING_CONTEXT_TEST_UTILS

#include "erp/model/ProfileType.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/model/DateTime.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace model
{
class Bundle;
class ErxReceipt;
}

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

std::set<fhirtools::ValidationError> validationResultFilter(const fhirtools::ValidationResults& validationResult,
                                                            const fhirtools::ValidatorOptions& options);

template<typename BundleT = model::ErxReceipt>
[[nodiscard]] BundleT
getValidatedErxReceiptBundle(std::string_view xmlDoc,
                             model::ProfileType profileType = model::ProfileType::Gem_erxReceiptBundle);

extern template model::Bundle getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);
extern template model::ErxReceipt getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);

class ShiftFhirResourceViewsGuard {
public:
    ShiftFhirResourceViewsGuard(const std::string& viewId, const fhirtools::Date& startDate);
    ShiftFhirResourceViewsGuard(const std::string& viewId, const date::sys_days& startDate);

private:
    std::vector<EnvironmentVariableGuard> envGuards;
};


} // namespace testutils

#endif// ERP_PROCESSING_CONTEXT_TEST_UTILS
