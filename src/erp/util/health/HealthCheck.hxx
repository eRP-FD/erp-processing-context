/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_HEALTH_HEALTHCHECK_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_HEALTH_HEALTHCHECK_HXX

#include "erp/pc/PcServiceContext.hxx"


class HealthCheck
{
public:
    static void update (PcServiceContext& context);

private:
    static void checkBna             (PcServiceContext& context);
    static void checkHsm             (PcServiceContext& context);
    static void checkCFdSigErp       (PcServiceContext& context);
    static void checkIdp             (PcServiceContext& context);
    static void checkPostgres        (PcServiceContext& context);
    static void checkRedis           (PcServiceContext& context);
    static void checkSeedTimer       (PcServiceContext& context);
    static void checkTeeTokenUpdater (PcServiceContext& context);
    static void checkTsl             (PcServiceContext& context);

    static void check (
        const ApplicationHealth::Service service,
        PcServiceContext& context,
        void (* checkAction)(PcServiceContext&));

    static void handleException(
        ApplicationHealth::Service service,
        PcServiceContext& context);
};


#endif
