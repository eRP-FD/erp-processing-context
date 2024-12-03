/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERMAIN_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERMAIN_HXX

#include "shared/MainState.hxx"
#include "shared/server/ThreadPool.hxx"

#include <functional>
#include <string_view>


class MedicationExporterFactories;
class MedicationExporterServiceContext;
class TslManager;
class XmlValidator;

boost::asio::awaitable<void> setupEpaClientPool(MedicationExporterServiceContext& serviceContext);

class MedicationExporterMain
{
public:
    class RunLoop;
    static MedicationExporterFactories createProductionFactories();

    static int
    runApplication(MedicationExporterFactories&& factories, MainStateCondition& state,
                   const std::function<void(MedicationExporterServiceContext&)>& postInitializationCallback = {});

    static bool waitForHealthUp(RunLoop& runLoop, MedicationExporterServiceContext& serviceContext);
    static bool testEpaEndpoints(MedicationExporterServiceContext& serviceContext);

    class RunLoop
    {
    public:
        enum class Reschedule : uint8_t
        {
            Delayed,
            Immediate
        };

        RunLoop();
        void serve(const std::shared_ptr<MedicationExporterServiceContext>& serviceContext, const size_t threadCount);
        void shutDown();
        bool isStopped() const;
        ThreadPool& getThreadPool();

    private:
        ThreadPool mWorkerThreadPool;
    };
};


#endif//ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERMAIN_HXX
