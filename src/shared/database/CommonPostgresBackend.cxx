#include "shared/database/CommonPostgresBackend.hxx"
#include "DatabaseModel.hxx"
#include "erp/database/ErpDatabaseModel.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

namespace
{
#define QUERY(name, query) const CommonPostgresBackend::QueryDefinition name = {# name, query};

    QUERY(retrieveSchemaVersionQuery,
          "SELECT value FROM erp.config WHERE parameter = 'schema_version'")

    QUERY(insertOrReturnAccountSalt, R"--(
          SELECT erp.insert_or_return_account_salt($1::bytea, $2::smallint, $3::integer, $4::bytea))--")

    QUERY(insertAuditEventData, "INSERT INTO erp.auditevent (id, kvnr_hashed, event_id, action, agent_type, observer, "
                                "prescription_id, prescription_type, metadata, blob_id) "
                                "VALUES (erp.gen_suuid($1), $2, $3, $4, $5, $6, $7, $8, $9, $10)"
                                "RETURNING id")

    QUERY(retrieveSaltForAccount, R"--(
          SELECT salt FROM erp.account WHERE account_id = $1 AND master_key_type = $2 AND blob_id = $3)--")

#undef QUERY
}


CommonPostgresBackend::CommonPostgresBackend(PostgresConnection& connection, const std::string_view& connectionString)
{
    connection.setConnectionString(connectionString);
    connection.connectIfNeeded();
    mTransaction = connection.createTransaction();
}

CommonPostgresBackend::~CommonPostgresBackend (void) = default;

void CommonPostgresBackend::commitTransaction()
{
    TVLOG(1) << "committing transaction";
    try
    {
        mTransaction->commit();
        mTransaction.reset();
    }
    catch (const pqxx::in_doubt_error& /*inDoubtError*/)
    {
        // Exception that might be thrown in rare cases where the connection to the database is lost while finishing a
        // database transaction, and there's no way of telling whether it was actually executed by the backend. In this
        // case the database is left in an indeterminate (but consistent) state, and only manual inspection will tell
        // which is the case.
        constexpr std::string_view error = "Connection to database lost during committing a transaction. The "
                                           "transaction may or may not have been successful";
        TLOG(WARNING) << error;
        ErpFail(HttpStatus::InternalServerError, std::string(error));
    }
    catch (...)
    {
        constexpr std::string_view error = "Error committing database transaction";
        TLOG(WARNING) << error;
        ErpFail(HttpStatus::InternalServerError, std::string(error));
    }
}

bool CommonPostgresBackend::isCommitted() const
{
    return !mTransaction;
}

void CommonPostgresBackend::closeConnection()
{
    TVLOG(1) << "closing connection to database";
    mTransaction.reset();
    connection().close();
    TVLOG(2) << "connection closed";
}

std::string CommonPostgresBackend::retrieveSchemaVersion()
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveSchemaVersionQuery.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:retrieveSchemaVersion");

    const auto results = mTransaction->exec(retrieveSchemaVersionQuery.query);
    TVLOG(2) << "got " << results.size() << " results";

    Expect(results.size() == 1, "Exactly one database schema version entry expected");
    Expect(!results[0].at(0).is_null(), "Database schema version must not be null");

    return results[0].at(0).template as<std::string>();
}

std::optional<DatabaseConnectionInfo> CommonPostgresBackend::getConnectionInfo() const
{
    return connection().getConnectionInfo();
}

void CommonPostgresBackend::checkCommonPreconditions() const
{
    Expect3(mTransaction, "Transaction already committed", std::logic_error);
}

std::optional<db_model::Blob> CommonPostgresBackend::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                             db_model::MasterKeyType masterKeyType,
                                                                             BlobId blobId, const db_model::Blob& salt)
{
    checkCommonPreconditions();
    TVLOG(2) << ::insertOrReturnAccountSalt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:insertOrReturnAccountSalt");

    auto result =
        mTransaction->exec_params(::insertOrReturnAccountSalt.query, accountId.binarystring(),
                                  gsl::narrow<int>(masterKeyType), gsl::narrow<int32_t>(blobId), salt.binarystring());
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expected exactly one row.");
    Expect(result.front().size() == 1, "Expected exactly one column.");
    if (result[0][0].is_null())
    {
        return std::nullopt;
    }
    return std::make_optional<db_model::Blob>(result[0][0].as<db_model::postgres_bytea>());
}

std::optional<db_model::Blob> CommonPostgresBackend::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                          db_model::MasterKeyType masterKeyType,
                                                                          BlobId blobId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::retrieveSaltForAccount.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:retrieveSaltForAccount");

    auto result = mTransaction->exec_params(::retrieveSaltForAccount.query, accountId.binarystring(),
                                            gsl::narrow<int>(masterKeyType), gsl::narrow<int32_t>(blobId));
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Expected at most one salt");
    if (result.empty())
    {
        return std::nullopt;
    }
    return db_model::Blob(result[0].at(0).as<db_model::postgres_bytea>());
}

std::string CommonPostgresBackend::storeAuditEventData(db_model::AuditData& auditData)
{
    checkCommonPreconditions();
    TVLOG(2) << insertAuditEventData.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:storeAuditEventData");

    const std::string actionString(1, static_cast<char>(auditData.action));
    const auto recorded = model::Timestamp::now();

    const pqxx::result result = mTransaction->exec_params(
        insertAuditEventData.query, recorded.toXsDateTime(), auditData.insurantKvnr.binarystring(),
        static_cast<std::int16_t>(auditData.eventId), actionString, static_cast<std::int16_t>(auditData.agentType),
        auditData.deviceId,
        auditData.prescriptionId.has_value() ? auditData.prescriptionId->toDatabaseId() : std::optional<int64_t>(),
        auditData.prescriptionId.has_value()
            ? static_cast<int16_t>(magic_enum::enum_integer(auditData.prescriptionId->type()))
            : std::optional<int16_t>(),
        auditData.metaData.has_value() ? auditData.metaData->binarystring()
                                       : std::optional<db_model::postgres_bytea_view>(),
        auditData.blobId);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1 && result.front().size() == 1, "Expected one result");
    auto sid = result.front().at(0).as<std::string>();
    Uuid id(sid);
    auditData.id = id;
    auditData.recorded = recorded;

    return id;
}
