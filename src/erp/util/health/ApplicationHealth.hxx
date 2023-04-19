/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_APPLICATIONHEALTH_HXX
#define ERP_PROCESSING_CONTEXT_APPLICATIONHEALTH_HXX

#include "erp/model/Health.hxx"

#include <mutex>
#include <unordered_map>
#include <vector>


/**
 * Container for health check results of individual services.
 * The up/down status of the services is updated by a health check which is triggered by requests to the
 * /health endpoint.
 */
class ApplicationHealth
{
public:
    enum class Service
    {
        Bna,
        Hsm,
        Idp,
        Postgres,
        PrngSeed,
        Redis,
        TeeToken,
        Tsl,
        CFdSigErp
    };
    enum class Status
    {
        Up,
        Down,
        Skipped,
        Unknown
    };
    enum class ServiceDetail
    {
        HsmDevice,
        CFdSigErpTimestamp,
        CFdSigErpPolicy,
        CFdSigErpExpiry,
        DBConnectionInfo
    };

    struct State
    {
        Status status{Status::Unknown};
        std::map<ServiceDetail, std::string> serviceDetails; // Details like HSM device.
        std::optional<std::string> details; // Details like reason for being down or being skipped.
    };

    static std::string_view toString (Service service);
    static std::string_view toString (Status status);

    void up (Service service);
    void down (Service service, std::optional<std::string_view> rootCause);
    void skip (Service service, std::string_view reason);

    /**
     * isUp, in its various forms, really is isUpOrSkipped. I.e. skipped services are counted as up.
     */
    bool isUp (void) const;
    bool isUp (Service service) const;

    void setServiceDetails (Service service, ServiceDetail key, const std::string& details);
    std::vector<Service> getDownServices (void) const;

    model::Health model (void) const;
    /**
     * Return a string that contains a comma separated list of service names that are currently down.
     * Empty when all services are up.
     */
    std::string downServicesString (void) const;

private:
    mutable std::mutex mMutex;
    std::unordered_map<Service, State> mStates;

    /// Helper function to retrieve a status in a format suitable as second argument for one of the Health::set*Status() functions.
    std::string_view getUpDownStatus (Service service) const;

    /// Helper function to retrieve the service details in a format suitable as third argument for one of the Health::setHsmStatus() functions.
    [[nodiscard]] std::string getServiceDetail(Service service, ServiceDetail key) const;

    /// Helper function to retrieve a details in a format suitable as third argument for one of the Health::set*Status() functions.
    std::optional<std::string_view> getDetails (Service service) const;

    std::string_view status_noLock() const;
    bool isUp_noLock (Service service) const;
    std::string downServicesString_noLock (void) const;
    bool servicesUp_noLock() const;
};


#endif
