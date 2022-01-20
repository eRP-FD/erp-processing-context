/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/health/ApplicationHealth.hxx"
#include "erp/model/Health.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"


namespace {
    constexpr size_t updateLogLevel = 2;
    constexpr size_t newStatusLogLevel = 1;

    void showUpdatedStatus (std::string&& downServicesString)
    {
        if (downServicesString.empty())
            TVLOG(newStatusLogLevel) << "updated health check status is UP";
        else
            TVLOG(newStatusLogLevel) << "updated health check status is DOWN, list of DOWN services: " << downServicesString;
    }
}


std::string_view ApplicationHealth::toString (const Service service)
{
    switch(service)
    {
        case Service::Bna:       return model::Health::bna;
        case Service::Hsm:       return model::Health::hsm;
        case Service::Idp:       return model::Health::idp;
        case Service::Postgres:  return model::Health::postgres;
        case Service::PrngSeed:  return model::Health::seedTimer;
        case Service::Redis:     return model::Health::redis;
        case Service::TeeToken:  return model::Health::teeTokenUpdater;
        case Service::Tsl:       return model::Health::tsl;
        case Service::CFdSigErp: return model::Health::cFdSigErp;
    }
    Fail("unhandled health service");
}


std::string_view ApplicationHealth::toString (Status status)
{
    switch(status)
    {
        case Status::Up:      return "Up";
        case Status::Down:    return "Down";
        case Status::Skipped: return "Skipped";
        case Status::Unknown: return "Unknown";
    }
    Fail("unhandled status");
}

void ApplicationHealth::up (Service service)
{
    std::lock_guard lock (mMutex);

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end() || iterator->second.status != Status::Up)
    {
        TVLOG(updateLogLevel) << "health-check " << toString(service) << ": UP (was "
                              << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
                              << ")";

        mStates[service] = {Status::Up, std::nullopt, std::nullopt};

        showUpdatedStatus(downServicesString_noLock());
    }
}


void ApplicationHealth::down (Service service, std::optional<std::string_view> rootCause)
{
    std::lock_guard lock (mMutex);

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end() || iterator->second.status != Status::Down)
    {
        TVLOG(updateLogLevel) << "health-check " << toString(service) << ": DOWN (was "
            << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
            << "; " << rootCause.value_or("unknown root cause") << ")";

        mStates[service] = {Status::Down, std::nullopt, std::string(rootCause.value_or(""))};

        showUpdatedStatus(downServicesString_noLock());
    }
}


void ApplicationHealth::skip (Service service, std::string_view reason)
{
    std::lock_guard lock (mMutex);

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end() || iterator->second.status != Status::Skipped)
    {
        TVLOG(updateLogLevel) << "health-check " << toString(service) << ": SKIPPED (was "
                              << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
                              << "; " << reason << ")";

        mStates[service] = {Status::Skipped, std::nullopt, "Skipped: " + std::string(reason)};

        showUpdatedStatus(downServicesString_noLock());
    }
}


bool ApplicationHealth::isUp (void) const
{
    std::lock_guard lock (mMutex);

    return status_noLock() == model::Health::up;
}


std::string_view ApplicationHealth::status_noLock() const
{
    const bool& isShuttingDown = TerminationHandler::instance().isShuttingDown();
    if (isShuttingDown)
    {
        return model::Health::shutdown;
    }
    return servicesUp_noLock() ? model::Health::up : model::Health::down;
}

bool ApplicationHealth::servicesUp_noLock() const
{
    return isUp_noLock(Service::Bna)
            && isUp_noLock(Service::Hsm)
            && isUp_noLock(Service::Idp)
            && isUp_noLock(Service::Postgres)
            && isUp_noLock(Service::Redis)
            && isUp_noLock(Service::PrngSeed)
            && isUp_noLock(Service::TeeToken)
            && isUp_noLock(Service::Tsl);
}


bool ApplicationHealth::isUp (Service service) const
{
    std::lock_guard lock (mMutex);
    return isUp_noLock(service);
}


bool ApplicationHealth::isUp_noLock (Service service) const
{
    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
        return false;
    else
        return iterator->second.status == Status::Up || iterator->second.status == Status::Skipped;
}


void ApplicationHealth::setServiceDetails (Service service, std::optional<std::string> serviceDetails)
{
    std::lock_guard lock (mMutex);
    if (serviceDetails.has_value())
    {
        TVLOG(2) << "health-check " << toString(service) << ": details: "<< serviceDetails.value();
        mStates[service].serviceDetails = serviceDetails;
    }
}


std::vector<ApplicationHealth::Service> ApplicationHealth::getDownServices (void) const
{
    std::lock_guard lock (mMutex);

    std::vector<Service> services;
    for (const auto service : magic_enum::enum_values<Service>())
        if ( ! isUp_noLock(service))
            services.push_back(service);
    return services;
}


model::Health ApplicationHealth::model (void) const
{
    std::lock_guard lock (mMutex);

    model::Health health;

    health.setBnaStatus(getUpDownStatus(Service::Bna), getDetails(Service::Bna));
    health.setHsmStatus(getUpDownStatus(Service::Hsm), getServiceDetails(Service::Hsm), getDetails(Service::Hsm));
    health.setIdpStatus(getUpDownStatus(Service::Idp), getDetails(Service::Idp));
    health.setPostgresStatus(getUpDownStatus(Service::Postgres), getDetails(Service::Postgres));
    health.setRedisStatus(getUpDownStatus(Service::Redis), getDetails(Service::Redis));
    health.setSeedTimerStatus(getUpDownStatus(Service::PrngSeed), getDetails(Service::PrngSeed));
    health.setTeeTokenUpdaterStatus(getUpDownStatus(Service::TeeToken), getDetails(Service::TeeToken));
    health.setTslStatus(getUpDownStatus(Service::Tsl), getDetails(Service::Tsl));
    health.setCFdSigErpStatus(getUpDownStatus(Service::CFdSigErp), getServiceDetails(Service::CFdSigErp), getDetails(Service::CFdSigErp));

    health.setOverallStatus(status_noLock());

    return health;
}


std::string ApplicationHealth::downServicesString (void) const
{
    std::lock_guard lock (mMutex);
    return downServicesString_noLock();
}


std::string ApplicationHealth::downServicesString_noLock (void) const
{
    std::ostringstream s;
    for (const auto service : magic_enum::enum_values<Service>())
    {
        if ( ! isUp_noLock(service))
        {
            if (s.tellp() > 0)
                s << ", ";
            s << toString(service);
        }
    }
    return s.str();
}


std::string_view ApplicationHealth::getUpDownStatus (const Service service) const
{
    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
        return model::Health::down;
    else if (iterator->second.status == Status::Up)
        return model::Health::up;
    else
        return model::Health::down;
}


std::string_view ApplicationHealth::getServiceDetails (const Service service) const
{
    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
        return {};
    else if (iterator->second.serviceDetails.has_value())
        return iterator->second.serviceDetails.value();
    else
        return {};
}

std::optional<std::string_view> ApplicationHealth::getDetails (const Service service) const
{
    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
        return std::nullopt;
    else
        return iterator->second.details;
}

