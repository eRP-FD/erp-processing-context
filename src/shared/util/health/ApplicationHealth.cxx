/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ApplicationHealth.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/model/Health.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"


namespace {
    constexpr size_t updateLogLevel = 2;
    constexpr size_t newStatusLogLevel = 1;

    void showUpdatedStatus (const std::string_view& status, std::string&& downServicesString)
    {
        std::stringstream sMessage;
        sMessage << "updated health check status is " << status;
        if (!downServicesString.empty())
        {
            sMessage << ", list of DOWN services: " << downServicesString;
        }
        TVLOG(newStatusLogLevel) << sMessage.str();
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
        case Service::EventDb:   return model::Health::eventDb;
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

    if (not mEnabledServices.contains(service))
    {
        return;
    }

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end() || iterator->second.status != Status::Up)
    {
        TVLOG(updateLogLevel) << "health-check " << toString(service) << ": UP (was "
                              << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
                              << ")";

        mStates[service] = {Status::Up, {}, std::nullopt};

        showUpdatedStatus(status_noLock(), downServicesString_noLock());
    }
}


void ApplicationHealth::down (Service service, std::optional<std::string_view> rootCause)
{
    std::lock_guard lock (mMutex);

    if (not mEnabledServices.contains(service))
    {
        return;
    }

    const auto iterator = mStates.find(service);
    TLOG(ERROR) << "health-check " << toString(service) << ": DOWN (was "
                << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
                << "; " << rootCause.value_or("unknown root cause") << ")";
    if (iterator == mStates.end() || iterator->second.status != Status::Down)
    {
        mStates[service] = {Status::Down, {}, std::string(rootCause.value_or(""))};

        showUpdatedStatus(status_noLock(), downServicesString_noLock());
    }
}


void ApplicationHealth::skip (Service service, std::string_view reason)
{
    std::lock_guard lock (mMutex);

    if (not mEnabledServices.contains(service))
    {
        return;
    }

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end() || iterator->second.status != Status::Skipped)
    {
        TVLOG(updateLogLevel) << "health-check " << toString(service) << ": SKIPPED (was "
                              << (iterator == mStates.end() ? "not set" : toString(iterator->second.status))
                              << "; " << reason << ")";

        mStates[service] = {Status::Skipped, {}, "Skipped: " + std::string(reason)};

        showUpdatedStatus(status_noLock(), downServicesString_noLock());
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
    bool result = true;
    for (const Service& svc : mEnabledServices)
    {
        if (svc == Service::CFdSigErp)
        {
            continue;
        }
        result = isUp_noLock(svc);
        if (not result)
        {
            break;
        }
    }
    return result;
}


bool ApplicationHealth::isUp (Service service) const
{
    std::lock_guard lock (mMutex);
    return isUp_noLock(service);
}


bool ApplicationHealth::isUp_noLock (Service service) const
{
    if (not mEnabledServices.contains(service))
    {
        return true;
    }

    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
        return false;
    else
        return iterator->second.status == Status::Up || iterator->second.status == Status::Skipped;
}


void ApplicationHealth::setServiceDetails(Service service, ServiceDetail key, const std::string& details)
{
    std::lock_guard lock(mMutex);
    TVLOG(2) << "health-check " << toString(service) << ": key=" << magic_enum::enum_name(key)
             << ", details: " << details;
    mStates[service].serviceDetails[key] = details;
}


std::vector<ApplicationHealth::Service> ApplicationHealth::getDownServices (void) const
{
    std::lock_guard lock (mMutex);

    std::vector<Service> services;
    for (const auto service : mEnabledServices)
    {
        if ( ! isUp_noLock(service))
        {
            services.push_back(service);
        }
    }
    return services;
}


model::Health ApplicationHealth::model() const
{
    std::lock_guard lock(mMutex);

    model::Health health;

    if (mEnabledServices.contains(Service::Bna))
    {
        health.setBnaStatus(getUpDownStatus(Service::Bna), getServiceDetail(Service::Bna, ServiceDetail::TSLExpiry),
                            getServiceDetail(Service::Bna, ServiceDetail::TSLSequenceNumber),
                            getServiceDetail(Service::Bna, ServiceDetail::TSLId),
                            getServiceDetail(Service::Bna, ServiceDetail::TslHash), getDetails(Service::Bna));
    }

    if (mEnabledServices.contains(Service::Hsm))
    {
        health.setHsmStatus(getUpDownStatus(Service::Hsm), getServiceDetail(Service::Hsm, ServiceDetail::HsmDevice),
                            getDetails(Service::Hsm));
    }

    if (mEnabledServices.contains(Service::Idp))
    {
        health.setIdpStatus(getUpDownStatus(Service::Idp), getDetails(Service::Idp));
    }

    if (mEnabledServices.contains(Service::Postgres))
    {
        health.setPostgresStatus(getUpDownStatus(Service::Postgres),
                                 getServiceDetail(Service::Postgres, ServiceDetail::DBConnectionInfo),
                                 getDetails(Service::Postgres));
    }

    if (mEnabledServices.contains(Service::EventDb))
    {
        health.setEventDbStatus(getUpDownStatus(Service::EventDb),
                                getServiceDetail(Service::EventDb, ServiceDetail::DBConnectionInfo),
                                getDetails(Service::EventDb));
    }

    if (mEnabledServices.contains(Service::Redis))
    {
        health.setRedisStatus(getUpDownStatus(Service::Redis), getDetails(Service::Redis));
    }

    if (mEnabledServices.contains(Service::PrngSeed))
    {
        health.setSeedTimerStatus(getUpDownStatus(Service::PrngSeed), getDetails(Service::PrngSeed));
    }

    if (mEnabledServices.contains(Service::TeeToken))
    {
        health.setTeeTokenUpdaterStatus(getUpDownStatus(Service::TeeToken), getDetails(Service::TeeToken));
    }

    if (mEnabledServices.contains(Service::Tsl))
    {
        health.setTslStatus(getUpDownStatus(Service::Tsl), getServiceDetail(Service::Tsl, ServiceDetail::TSLExpiry),
                            getServiceDetail(Service::Tsl, ServiceDetail::TSLSequenceNumber),
                            getServiceDetail(Service::Tsl, ServiceDetail::TSLId),
                            getServiceDetail(Service::Tsl, ServiceDetail::TslHash), getDetails(Service::Tsl));
    }

    if (mEnabledServices.contains(Service::CFdSigErp))
    {
        health.setCFdSigErpStatus(getUpDownStatus(Service::CFdSigErp),
                                  getServiceDetail(Service::CFdSigErp, ServiceDetail::CFdSigErpTimestamp),
                                  getServiceDetail(Service::CFdSigErp, ServiceDetail::CFdSigErpPolicy),
                                  getServiceDetail(Service::CFdSigErp, ServiceDetail::CFdSigErpExpiry),
                                  getDetails(Service::CFdSigErp));
    }

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
    for (const auto service : mEnabledServices)
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
    if (iterator == mStates.end() || iterator->second.status != Status::Up)
    {
        return model::Health::down;
    }
    else
    {
        return model::Health::up;
    }
}


std::string ApplicationHealth::getServiceDetail(const ApplicationHealth::Service service,
                                                const ApplicationHealth::ServiceDetail key) const
{
    const auto iterator = mStates.find(service);
    if (iterator == mStates.end())
    {
        return {};
    }
    auto candidate = iterator->second.serviceDetails.find(key);
    if (candidate != iterator->second.serviceDetails.end())
    {
        return candidate->second;
    }
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

void ApplicationHealth::enableCheck(const Service& service)
{
    mEnabledServices.emplace(service);
}

void ApplicationHealth::enableChecks(const std::set<Service>& services)
{
    for (const auto& svc : services)
    {
        enableCheck(svc);
    }
}
