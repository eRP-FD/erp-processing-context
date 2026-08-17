/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#pragma once
#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/model/push/PushNotificationContext.hxx"

#undef Expect
#include <gmock/gmock.h>

class MainDatabaseFrontendMock : public exporter::MainDatabaseFrontendInterface
{
public:
    MOCK_METHOD(std::shared_ptr<Compression>, compressionInstance, (), (override));
    MOCK_METHOD((std::tuple<SafeString, BlobId>), auditEventKey, (const db_model::HashedKvnr& hashedKvnr), (override));
    MOCK_METHOD(std::string, storeAuditEventData, (const AuditDataCollector& auditDataCollector), (override));
    MOCK_METHOD(void, healthCheck, (), (override));
    MOCK_METHOD(std::optional<DatabaseConnectionInfo>, getConnectionInfo, (), (const, override));
    MOCK_METHOD(void, commitTransaction, (), (override));

    MOCK_METHOD(std::vector<model::PushNotificationContext>, retrievePushNotificationEventData,
                (const model::PushNotificationEvent& pushNotificationEvent), (override));
    MOCK_METHOD(void, updateEncryptionKey,
                (const db_model::HashedKvnr& kvnrHashed, const model::PushNotificationContext& pushNotificationEvent),
                (override));
    MOCK_METHOD(void, deletePushKey, (const model::HashedKvnr& hashedKvnr, const model::PushKey& pushKey), (const, override));
};

class MainDatabaseFrontendMockProxy : public exporter::MainDatabaseFrontendInterface
{
public:
    MainDatabaseFrontendMockProxy(gsl::not_null<MainDatabaseFrontendMock*> mock);

    std::shared_ptr<Compression> compressionInstance() override;
    std::tuple<SafeString, BlobId> auditEventKey(const db_model::HashedKvnr& hashedKvnr) override;
    std::string storeAuditEventData(const AuditDataCollector& auditDataCollector) override;
    void healthCheck() override;
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;
    void commitTransaction() override;

    std::vector<model::PushNotificationContext>
    retrievePushNotificationEventData(const model::PushNotificationEvent& pushNotificationEvent) override;
    void updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                             const model::PushNotificationContext& pushNotificationEvent) override;
    void deletePushKey(const model::HashedKvnr& hashedKvnr, const model::PushKey& pushKey) const override;

    gsl::not_null<MainDatabaseFrontendMock*> mDatabase;
};
