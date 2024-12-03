/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/MainDatabaseFrontend.hxx"

using namespace exporter;

MainDatabaseFrontend::MainDatabaseFrontend(std::unique_ptr<MainPostgresBackend>&& backend, HsmPool& hsmPool,
                                           KeyDerivation& keyDerivation)
    : mBackend(std::move(backend))
    , mCommonDatabaseFrontend(std::make_unique<CommonDatabaseFrontend>(hsmPool, keyDerivation))
{
}

MainDatabaseFrontend::~MainDatabaseFrontend() = default;

std::shared_ptr<Compression> MainDatabaseFrontend::compressionInstance()
{
    return CommonDatabaseFrontend::compressionInstance();
}

std::tuple<SafeString, BlobId> MainDatabaseFrontend::auditEventKey(const db_model::HashedKvnr& hashedKvnr)
{
    return mCommonDatabaseFrontend->auditEventKey(*mBackend, hashedKvnr);
}

std::string MainDatabaseFrontend::storeAuditEventData(model::AuditData& auditData)
{
    return mCommonDatabaseFrontend->storeAuditEventData(*mBackend, auditData);
}

void MainDatabaseFrontend::healthCheck()
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> MainDatabaseFrontend::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

void MainDatabaseFrontend::commitTransaction()
{
    mBackend->commitTransaction();
}
