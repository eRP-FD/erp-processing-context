/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/database/push/PushExporterBackend.hxx"

#include <memory>
#include <optional>
#include <string>

class PostgresConnection;
class PostgresConnectionParameters;
class PushEventDataCollector;
struct DatabaseConnectionInfo;

namespace pqxx
{
class transaction_base;

}

namespace model
{
class PrescriptionId;
}

class PushExporterPostgresBackend : public PushExporterBackend
{
public:
    struct QueryDefinition {
        const char* name{};
        std::string query{};
    };

    explicit PushExporterPostgresBackend(PostgresConnection& connection);

    void healthCheck() const override;

    void commitTransaction() override;
    bool isCommitted() const override;
    void closeConnection() override;

    PostgresConnection& connection() const;

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;
    void checkCommonPreconditions() const;
    const std::unique_ptr<pqxx::transaction_base>& transaction() const;

    static PostgresConnection& mainConnection();

    [[nodiscard]]
    static PostgresConnectionParameters defaultConnectParameters();

    void createPushEvent(const db_model::PushEventData& data) override;

private:
    void createPushEvent(const db_model::PushEventData& data, const model::PrescriptionId& prescriptionId) const;
    std::reference_wrapper<PostgresConnection> mConnection;
    std::unique_ptr<pqxx::transaction_base> mTransaction;
};
