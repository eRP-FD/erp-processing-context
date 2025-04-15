/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX
#define ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX

#include "MainPostgresBackend.hxx"
#include "shared/database/CommonDatabaseFrontend.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/model/AuditData.hxx"

namespace exporter
{

class MainDatabaseFrontend
{
public:
    using Factory = std::function<std::unique_ptr<MainDatabaseFrontend>(HsmPool&, KeyDerivation&)>;

    MainDatabaseFrontend(std::unique_ptr<exporter::MainPostgresBackend>&& backend, HsmPool& hsmPool,
                         KeyDerivation& keyDerivation);
    ~MainDatabaseFrontend();
    std::shared_ptr<Compression> compressionInstance();
    std::tuple<SafeString, BlobId> auditEventKey(const db_model::HashedKvnr& hashedKvnr);
    std::string storeAuditEventData(model::AuditData& auditData);
    void healthCheck();
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const;
    void commitTransaction();

private:
    std::unique_ptr<exporter::MainPostgresBackend> mBackend;
    std::unique_ptr<CommonDatabaseFrontend> mCommonDatabaseFrontend;
};

};// namespace exporter;

#endif//ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX
