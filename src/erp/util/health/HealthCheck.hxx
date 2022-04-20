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
    static void update (SessionContext& session);

private:
    static void checkBna             (SessionContext& session);
    static void checkHsm             (SessionContext& session);
    static void checkCFdSigErp       (SessionContext& session);
    static void checkIdp             (SessionContext& session);
    static void checkPostgres        (SessionContext& session);
    static void checkRedis           (SessionContext& session);
    static void checkSeedTimer       (SessionContext& session);
    static void checkTeeTokenUpdater (SessionContext& session);
    static void checkTsl             (SessionContext& session);

    static void check (
        const ApplicationHealth::Service service,
        SessionContext& session,
        void (* checkAction)(SessionContext&));

    static void handleException(
        ApplicationHealth::Service service,
        SessionContext& session);
};


#endif
