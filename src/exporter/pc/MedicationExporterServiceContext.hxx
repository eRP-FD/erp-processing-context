/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX

#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/server/BaseServiceContext.hxx"

class Configuration;
class MedicationExporterFactories;
class Tee3ClientPool;

namespace boost::asio
{
class io_context;
}

class MedicationExporterServiceContext : public BaseServiceContext
{
public:
    MedicationExporterServiceContext(boost::asio::io_context& ioContext, const Configuration& configuration, MedicationExporterFactories&& factories);
    ~MedicationExporterServiceContext(void) override = default;

    MedicationExporterServiceContext(const MedicationExporterServiceContext& other) = delete;
    MedicationExporterServiceContext& operator=(const MedicationExporterServiceContext& other) = delete;
    MedicationExporterServiceContext& operator=(MedicationExporterServiceContext&& other) = delete;

    std::unique_ptr<MedicationExporterDatabaseFrontendInterface> medicationExporterDatabaseFactory();
    std::unique_ptr<exporter::MainDatabaseFrontend> erpDatabaseFactory();

    /**
     * io context to be used for io requests (e.g. https requests)
     */
    boost::asio::io_context& ioContext();

    std::shared_ptr<Tee3ClientPool> teeClientPool();

private:
    boost::asio::io_context& mIoContext;
    MedicationExporterDatabaseFrontendInterface::Factory mExporterDatabaseFactory;
    exporter::MainDatabaseFrontend::Factory mErpDatabaseFactory;
    std::shared_ptr<Tee3ClientPool> mTeeClientPool;
};

#endif//ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX
