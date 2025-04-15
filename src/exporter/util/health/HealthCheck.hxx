/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SHARED_UTIL_HEALTH_HEALTHCHECK_HXX
#define ERP_PROCESSING_CONTEXT_SHARED_UTIL_HEALTH_HEALTHCHECK_HXX

#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/util/health/ApplicationHealth.hxx"


class HealthCheck
{
public:
    static void update(MedicationExporterServiceContext& context);

private:
    static void checkBna(MedicationExporterServiceContext& context);
    static void checkHsm(MedicationExporterServiceContext& context);
    static void checkPostgres(MedicationExporterServiceContext& context);
    static void checkSeedTimer(MedicationExporterServiceContext& context);
    static void checkTeeTokenUpdater(MedicationExporterServiceContext& context);
    static void checkTsl(MedicationExporterServiceContext& context);
    static void checkEventDb(MedicationExporterServiceContext& context);

    static void check(const ApplicationHealth::Service service, MedicationExporterServiceContext& context,
                      void (*checkAction)(MedicationExporterServiceContext&));

    static void handleException(ApplicationHealth::Service service, MedicationExporterServiceContext& context);
};


#endif
