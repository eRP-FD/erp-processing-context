/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTILS
#define ERP_PROCESSING_CONTEXT_TEST_UTILS

#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/model/ProfileType.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

namespace model
{
class Bundle;
class ErxReceipt;
class FhirResourceBase;
class NumberAsStringParserDocument;
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

void validate(const model::FhirResourceBase& resource);
std::unique_ptr<model::FhirResourceBase> createResource(model::NumberAsStringParserDocument doc);
std::unique_ptr<model::FhirResourceBase> createResourceFromJson(std::string_view jsonStr);
std::unique_ptr<model::FhirResourceBase> createResourceFromXml(std::string_view xmlStr);
std::unique_ptr<model::FhirResourceBase> createResource(std::string_view doc);

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
