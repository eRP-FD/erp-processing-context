/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_COMMONPOSTGRESBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_COMMONPOSTGRESBACKEND_HXX

#include "shared/database/DatabaseBackend.hxx"
#include "shared/database/PostgresConnection.hxx"

#include "shared/database/DatabaseModel.hxx"

#include <memory>
#include <string>
#include <pqxx/binarystring>
#include <pqxx/transaction_base>

namespace pqxx {class connection;}

class CommonPostgresBackend : virtual public DatabaseBackend
{
public:
    CommonPostgresBackend(PostgresConnection& connection, const std::string_view& connectionString,
                          TransactionMode mode = TransactionMode::transaction);
    CommonPostgresBackend() = delete;
    ~CommonPostgresBackend (void) override;

    void commitTransaction() override;
    bool isCommitted() const override;
    void closeConnection() override;

    std::string retrieveSchemaVersion() override;

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;

    struct QueryDefinition {
        const char* name{};
        std::string query{};
    };

    virtual PostgresConnection& connection() const = 0;

    [[nodiscard]]
    std::string storeAuditEventData(db_model::AuditData& auditData) override;

    std::optional<db_model::Blob> insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                            db_model::MasterKeyType masterKeyType, BlobId blobId,
                                                            const db_model::Blob& salt) override;

    std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                         db_model::MasterKeyType masterKeyType, BlobId blobId) override;

protected:
    void checkCommonPreconditions() const;

    const std::unique_ptr<pqxx::transaction_base>& transaction() const;

private:
    std::unique_ptr<pqxx::transaction_base> mTransaction;
};

using QueryDefinition = CommonPostgresBackend::QueryDefinition;

#endif // ERP_PROCESSING_CONTEXT_DATABASE_COMMONPOSTGRESBACKEND_HXX
