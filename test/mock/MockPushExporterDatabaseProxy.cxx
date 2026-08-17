/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockPushExporterDatabaseProxy.hxx"
#include "MockPushExporterDatabase.hxx"
#include "erp/database/push/PushEventDataCollector.hxx"
#include "shared/database/DatabaseConnectionInfo.hxx"

MockPushExporterDatabaseProxy::MockPushExporterDatabaseProxy(std::shared_ptr<MockPushExporterDatabase> mockDatabase)
    : mMockDatabase(mockDatabase)
{
}

void MockPushExporterDatabaseProxy::healthCheck() const
{
    mMockDatabase->healthCheck();
}

std::optional<DatabaseConnectionInfo> MockPushExporterDatabaseProxy::getConnectionInfo() const
{
    return std::nullopt;
}

void MockPushExporterDatabaseProxy::closeConnection() {

}

void MockPushExporterDatabaseProxy::createPushEvent(const db_model::PushEventData&) {

}

void MockPushExporterDatabaseProxy::commitTransaction() {

}

bool MockPushExporterDatabaseProxy::isCommitted() const {
    return true;
}
