/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_HEALTH_HEALTHCHECK_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_HEALTH_HEALTHCHECK_HXX

#include "erp/pc/PcServiceContext.hxx"


class HealthCheck
{
public:
    static void update (SessionContext<PcServiceContext>& session);

private:
    static void checkBna             (SessionContext<PcServiceContext>& session);
    static void checkHsm             (SessionContext<PcServiceContext>& session);
    static void checkIdp             (SessionContext<PcServiceContext>& session);
    static void checkPostgres        (SessionContext<PcServiceContext>& session);
    static void checkRedis           (SessionContext<PcServiceContext>& session);
    static void checkSeedTimer       (SessionContext<PcServiceContext>& session);
    static void checkTeeTokenUpdater (SessionContext<PcServiceContext>& session);
    static void checkTsl             (SessionContext<PcServiceContext>& session);

    static void check (
        const ApplicationHealth::Service service,
        SessionContext<PcServiceContext>& session,
        void (* checkAction)(SessionContext<PcServiceContext>&));

    static void handleException(
        ApplicationHealth::Service service,
        SessionContext<PcServiceContext>& session);
};


#endif
