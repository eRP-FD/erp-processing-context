/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX

#include "exporter/database/CommitGuard.hxx"
#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/util/HeaderLog.hxx"

#include <unordered_map>

class Configuration;
class MedicationExporterFactories;
class Tee3ClientPool;
class HttpsClientPool;
enum class TransactionMode : uint8_t;

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

    template<typename FuncT>
    std::invoke_result_t<FuncT, MedicationExporterDatabaseFrontendInterface&> transaction(TransactionMode mode,
                                                                                          FuncT&& func);

private:
    std::unique_ptr<MedicationExporterDatabaseFrontendInterface>
    medicationExporterDatabaseFactory(TransactionMode mode);

    boost::asio::io_context& mIoContext;
    MedicationExporterDatabaseFrontendInterface::Factory mExporterDatabaseFactory;
    exporter::MainDatabaseFrontend::Factory mErpDatabaseFactory;
    std::shared_ptr<Tee3ClientPool> mTeeClientPool;
    std::unordered_map<std::string, std::shared_ptr<HttpsClientPool>> mHttpsClientPools;
    gsl::not_null<std::shared_ptr<exporter::RuntimeConfiguration>> mRuntimeConfiguration;
#ifdef FRIEND_TEST
    FRIEND_TEST(CommitGuardTest, only_one_transaction_allowed);
    FRIEND_TEST(CommitGuardTest, create_and_query);
#endif
};

template<typename FuncT>
std::invoke_result_t<FuncT, MedicationExporterDatabaseFrontendInterface&>
MedicationExporterServiceContext::transaction(TransactionMode mode, FuncT&& func)
{
    try
    {
        CommitGuard db{medicationExporterDatabaseFactory(mode)};
        return std::invoke(std::forward<FuncT>(func), db.db());
    }
    catch (pqxx::broken_connection&)
    {
        // reconnect is handled by PostgresConnection::createTransaction
        HeaderLog::vlog(1, "connection lost during transaction - retrying");
        CommitGuard db{medicationExporterDatabaseFactory(mode)};
        return std::invoke(std::forward<FuncT>(func), db.db());
    }
}


#endif//ERP_PROCESSING_CONTEXT_EXPORTER_MEDICATIONEXPORTERSERVICECONTEXT_HXX
