/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "test/exporter/mock/MainDatabaseFrontendMock.hxx"

MainDatabaseFrontendMockProxy::MainDatabaseFrontendMockProxy(gsl::not_null<MainDatabaseFrontendMock*> mock)
    : mDatabase(mock)
{
}
std::shared_ptr<Compression> MainDatabaseFrontendMockProxy::compressionInstance()
{
    return mDatabase->compressionInstance();
}
std::tuple<SafeString, BlobId> MainDatabaseFrontendMockProxy::auditEventKey(const db_model::HashedKvnr& hashedKvnr)
{
    return mDatabase->auditEventKey(hashedKvnr);
}
std::string MainDatabaseFrontendMockProxy::storeAuditEventData(const AuditDataCollector& auditDataCollector)
{
    return mDatabase->storeAuditEventData(auditDataCollector);
}
void MainDatabaseFrontendMockProxy::healthCheck()
{
    mDatabase->healthCheck();
}
std::optional<DatabaseConnectionInfo> MainDatabaseFrontendMockProxy::getConnectionInfo() const
{
    return mDatabase->getConnectionInfo();
}
void MainDatabaseFrontendMockProxy::commitTransaction()
{
    mDatabase->commitTransaction();
}
std::vector<model::PushNotificationContext> MainDatabaseFrontendMockProxy::retrievePushNotificationEventData(
    const model::PushNotificationEvent& pushNotificationEvent)
{
    return mDatabase->retrievePushNotificationEventData(pushNotificationEvent);
}
void MainDatabaseFrontendMockProxy::updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                                                        const model::PushNotificationContext& pushNotificationEvent)
{
    mDatabase->updateEncryptionKey(kvnrHashed, pushNotificationEvent);
}
void MainDatabaseFrontendMockProxy::deletePushKey(const model::HashedKvnr& hashedKvnr,
                                                  const model::PushKey& pushKey) const
{
    mDatabase->deletePushKey(hashedKvnr, pushKey);
}
