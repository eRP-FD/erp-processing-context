/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX

#include "shared/database/DatabaseConnectionInfo.hxx"
#include "shared/database/DatabaseModel.hxx"

#include <optional>
#include <string>


class DatabaseBackend
{
public:
    virtual ~DatabaseBackend(void) = default;

    virtual void commitTransaction() = 0;
    virtual void closeConnection() = 0;
    virtual bool isCommitted() const = 0;

    virtual std::string retrieveSchemaVersion() = 0;

    virtual void healthCheck() = 0;

    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const
    {
        return std::nullopt;
    }

    virtual std::string storeAuditEventData(db_model::AuditData& auditData) = 0;
    [[nodiscard]]
    virtual std::optional<db_model::Blob> retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                 db_model::MasterKeyType masterKeyType,
                                                                 BlobId blobId) = 0;

    /// @brief try to insert a new salt into the database
    /// @returns empty if the salt was insert; if the insert failed, returns the salt already contained in the DB
    [[nodiscard]]
    virtual std::optional<db_model::Blob>
    insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                              db_model::MasterKeyType masterKeyType,
                              BlobId blobId,
                              const db_model::Blob& salt) = 0;
};

#endif//ERP_PROCESSING_CONTEXT_DATABASEBACKEND_HXX
