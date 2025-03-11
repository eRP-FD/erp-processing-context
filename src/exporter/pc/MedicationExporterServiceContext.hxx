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

#include <unordered_map>

class Configuration;
class MedicationExporterFactories;
class Tee3ClientPool;
class HttpsClientPool;

namespace boost::asio
{
class io_context;
}
namespace exporter
{
class RuntimeConfigurationGetter;
class RuntimeConfigurationSetter;
class RuntimeConfiguration;
}

class MedicationExporterServiceContext : public BaseServiceContext
{
public:
    MedicationExporterServiceContext(boost::asio::io_context& ioContext, const Configuration& configuration,
                                     const MedicationExporterFactories& factories);
    ~MedicationExporterServiceContext() override;

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
    std::shared_ptr<HttpsClientPool> httpsClientPool(const std::string& hostname);

    std::unique_ptr<exporter::RuntimeConfigurationGetter> getRuntimeConfigurationGetter() const;
    std::unique_ptr<exporter::RuntimeConfigurationSetter> getRuntimeConfigurationSetter() const;
    std::shared_ptr<const exporter::RuntimeConfiguration> getRuntimeConfiguration() const;

private:
    boost::asio::io_context& mIoContext;
    MedicationExporterDatabaseFrontendInterface::Factory mExporterDatabaseFactory;
    exporter::MainDatabaseFrontend::Factory mErpDatabaseFactory;
    std::shared_ptr<Tee3ClientPool> mTeeClientPool;
    std::unordered_map<std::string, std::shared_ptr<HttpsClientPool>> mHttpsClientPools;
    gsl::not_null<std::shared_ptr<exporter::RuntimeConfiguration>> mRuntimeConfiguration;
};

#endif//ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX
