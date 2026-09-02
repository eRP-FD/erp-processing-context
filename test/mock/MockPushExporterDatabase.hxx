/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

class HsmPool;

#include "erp/database/push/PushExporterBackend.hxx"
#include "shared/database/DatabaseConnectionInfo.hxx"

#undef Expect
#include <gmock/gmock-function-mocker.h>

class MockPushExporterDatabase : public PushExporterBackend
{
 public:
    MockPushExporterDatabase();
    ~MockPushExporterDatabase() override = default;
    MOCK_METHOD(void, healthCheck, (), (const, override));
    MOCK_METHOD(void, commitTransaction, (), (override));
    MOCK_METHOD(void, closeConnection, (), (override));
    MOCK_METHOD(bool, isCommitted, (), (const, override));
    MOCK_METHOD(std::optional<DatabaseConnectionInfo>, getConnectionInfo, (), (const, override));
    MOCK_METHOD(void, createPushEvent, (const db_model::PushEventData& data), (override));

};
