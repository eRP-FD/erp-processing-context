/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERMAIN_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERMAIN_HXX

#include "shared/MainState.hxx"
#include "shared/server/ThreadPool.hxx"

#include <functional>
#include <memory>


class MedicationExporterFactories;
class MedicationExporterServiceContext;
class RunLoopScheduler;
class TslManager;
class XmlValidator;

boost::asio::awaitable<void> setupEpaClientPool(std::shared_ptr<MedicationExporterServiceContext> serviceContext);

class MedicationExporterMain
{
public:
    static MedicationExporterFactories createProductionFactories();

    static int
    runApplication(MedicationExporterFactories&& factories, MainStateCondition& state,
                   const std::function<void(MedicationExporterServiceContext&)>& postInitializationCallback = {});

    static bool waitForHealthUp(RunLoopScheduler& runLoop,
                                const std::shared_ptr<MedicationExporterServiceContext>& serviceContext);
    static bool probeEpaEndpoints(MedicationExporterServiceContext& serviceContext);
    static bool probeTRezeptEndpoints(MedicationExporterServiceContext& serviceContext);
    static bool probePushGatewayEndpoints(MedicationExporterServiceContext& serviceContext);

};


#endif
