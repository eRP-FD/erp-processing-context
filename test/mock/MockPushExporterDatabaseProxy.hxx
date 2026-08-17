/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/database/push/PushExporterBackend.hxx"

#include <memory>

class MockPushExporterDatabase;

class MockPushExporterDatabaseProxy : public PushExporterBackend
{
public:
    explicit MockPushExporterDatabaseProxy(std::shared_ptr<MockPushExporterDatabase> mockDatabase);

    ~MockPushExporterDatabaseProxy() override = default;

    void healthCheck() const override;
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;

    void closeConnection() override;
    void createPushEvent(const db_model::PushEventData& data) override;

    void commitTransaction() override;
    bool isCommitted() const override;

private:
    std::shared_ptr<MockPushExporterDatabase> mMockDatabase;
};
