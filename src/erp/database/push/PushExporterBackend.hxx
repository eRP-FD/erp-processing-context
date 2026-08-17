/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include <optional>


struct DatabaseConnectionInfo;
namespace db_model
{
    class PushEventData;
}

class PushExporterBackend
{
public:
    virtual ~PushExporterBackend() = default;

    virtual void healthCheck() const = 0;
    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const = 0;

    virtual void closeConnection() = 0;
    virtual void createPushEvent(const db_model::PushEventData& data) = 0;

    virtual void commitTransaction() = 0;
    virtual bool isCommitted() const = 0;
};
