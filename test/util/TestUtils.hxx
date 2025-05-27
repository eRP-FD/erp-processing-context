/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTILS
#define ERP_PROCESSING_CONTEXT_TEST_UTILS

#include "shared/model/ProfileType.hxx"
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
class UnspecifiedResource;
}

namespace testutils
{

template<typename T>
static void waitFor(T predicate, const std::chrono::milliseconds& timeout = std::chrono::milliseconds(2000))
{
    auto end = timeout / std::chrono::milliseconds(20);
    for (decltype(end) retries = 0; retries < end; ++retries)
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

void bestEffortValidate(const model::UnspecifiedResource& res);

template<typename BundleT = model::ErxReceipt>
[[nodiscard]] BundleT
getValidatedErxReceiptBundle(std::string_view xmlDoc,
                             model::ProfileType profileType = model::ProfileType::GEM_ERP_PR_Bundle);

extern template model::Bundle getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);
extern template model::ErxReceipt getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);

std::string shiftDate(const std::string& realDate);

class ShiftFhirResourceViewsGuard {
public:
    class AsConfiguredTag {};
    static constexpr AsConfiguredTag asConfigured{};

    ShiftFhirResourceViewsGuard(const AsConfiguredTag&);
    ShiftFhirResourceViewsGuard(const std::string& viewId, const fhirtools::Date& startDate);
    static std::chrono::days calculateOffset(const std::string& viewId, const fhirtools::Date& startDate);
    ShiftFhirResourceViewsGuard(const std::chrono::days& offset);
    ShiftFhirResourceViewsGuard(const std::string& viewId, const date::sys_days& startDate);

private:
    std::vector<EnvironmentVariableGuard> envGuards;
};


} // namespace testutils

#endif// ERP_PROCESSING_CONTEXT_TEST_UTILS
