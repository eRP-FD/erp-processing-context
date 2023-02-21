/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/TestUtils.hxx"
#include "erp/model/Timestamp.hxx"

#include <chrono>

namespace testutils
{

std::vector<EnvironmentVariableGuard> getNewFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto yesterday = (model::Timestamp::now() - 24h).toXsDateTime();

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, yesterday);
    return envVars;
}


std::vector<EnvironmentVariableGuard> getOldFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto tomorrow = (model::Timestamp::now() + 24h).toXsDateTime();

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, tomorrow);
    return envVars;
}

std::vector<EnvironmentVariableGuard> getOverlappingFhirProfileEnvironment()
{
    using namespace std::chrono_literals;
    const auto tomorrow = (model::Timestamp::now() + 24h).toXsDateTime();
    const auto yesterday = (model::Timestamp::now() - 24h).toXsDateTime();

    std::vector<EnvironmentVariableGuard> envVars;
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL, tomorrow);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_RENDER_FROM, yesterday);
    envVars.emplace_back(ConfigurationKey::FHIR_PROFILE_VALID_FROM, yesterday);
    return envVars;
}

} // namespace testutils