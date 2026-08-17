/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/database/push/PushExporterBackend.hxx"

#undef Expect
#include <gmock/gmock.h>


class PushExporterMockBackend : public PushExporterBackend
{
public:
    MOCK_METHOD(void, commitTransaction, () );
    MOCK_METHOD(void, healthCheck, (), (const, override));
    MOCK_METHOD(std::optional<DatabaseConnectionInfo>, getConnectionInfo, (), (const, override));
    MOCK_METHOD(void, closeConnection, (), (override));
    MOCK_METHOD(bool, isCommitted, (), (const));
    MOCK_METHOD(std::string, retrieveSchemaVersion, ());
    MOCK_METHOD(void, healthCheck, ());
    MOCK_METHOD(void, createPushEvent, (const db_model::PushEventData& data), (override));
};
