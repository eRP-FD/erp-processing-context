/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/MainPostgresBackend.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "exporter/database/TaskEvent.hxx"
#include "shared/util/DurationConsumer.hxx"

#include <pqxx/pqxx>

namespace
{
#define QUERY(name, query) const QueryDefinition name = {#name, query};

QUERY(healthCheckQuery, "SELECT FROM erp.task LIMIT 1")

#undef QUERY

}// anonymous namespace

using namespace exporter;

MainPostgresBackend::MainPostgresBackend()
    : CommonPostgresBackend(threadConnection())
{
}

void MainPostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << healthCheckQuery.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck");
    const auto result = transaction()->exec(healthCheckQuery.query);
    TVLOG(2) << "got " << result.size() << " results";
}

PostgresConnection& MainPostgresBackend::connection() const
{
    return threadConnection();
}

std::vector<db_model::AppRegistration>
MainPostgresBackend::retrieveAppRegistrations(const db_model::HashedKvnr& kvnrHashed,
                                              const std::string& channelId) const
{
    checkCommonPreconditions();
    static constexpr auto sql = R"(
SELECT pushkey_hashed, app_id_hashed, kvnr_hashed, blob_id, salt, payload, url, encryption_key, subscribed_channel, EXTRACT(EPOCH FROM time_created), EXTRACT(EPOCH FROM last_modified)
FROM erp.app_registrations
WHERE (kvnr_hashed = $1) AND ($2::erp.push_notification_channel = ANY(subscribed_channel));
)";
    struct PushEventQueryIndexes {
        pqxx::row::size_type pushKeyHashed = 0;
        pqxx::row::size_type appIdHashed = 1;
        pqxx::row::size_type kvnrHashed = 2;
        pqxx::row::size_type blobId = 3;
        pqxx::row::size_type salt = 4;
        pqxx::row::size_type payload = 5;
        pqxx::row::size_type url = 6;
        pqxx::row::size_type encryptionKey = 7;
        pqxx::row::size_type subscribedChannel = 8;
        pqxx::row::size_type timeCreated = 9;
        pqxx::row::size_type lastModified = 10;
    };
    static constexpr PushEventQueryIndexes idx;

    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "retrieveappregistrations");
    std::vector<db_model::AppRegistration> registrations;
    const auto result = transaction()->exec(sql, pqxx::params{kvnrHashed.binarystring(), channelId});
    TVLOG(2) << "got " << result.size() << " results";
    for (const auto& row : result)
    {
        using MB = MedicationExporterPostgresBackend;
        registrations.emplace_back(
            MB::map<db_model::HashedId, db_model::postgres_bytea>(row, idx.pushKeyHashed, "pushkey_hashed is null"),
            MB::map<db_model::HashedId, db_model::postgres_bytea>(row, idx.appIdHashed, "app_id_hashed is null"),
            MB::map<db_model::HashedKvnr, db_model::postgres_bytea>(row, idx.kvnrHashed, "kvnr_hashed is null"),
            MB::map<BlobId>(row, idx.blobId, "blob_id is null"),
            MB::map<db_model::Blob, db_model::postgres_bytea>(row, idx.salt, "salt is null"),
            MB::map<db_model::EncryptedBlob, db_model::postgres_bytea>(row, idx.payload, "payload is null"),
            MB::map<std::string>(row, idx.url, "url is null"),
            MB::map<db_model::EncryptedBlob, db_model::postgres_bytea>(row, idx.encryptionKey,
                                                                       "encryption_key is null"),
            MB::map<model::Timestamp, double>(row, idx.timeCreated, "time_created is null"),
            MB::map<model::Timestamp, double>(row, idx.lastModified, "last_modified is null"));
    }
    return registrations;
}

// GEMREQ-start A_27405
void MainPostgresBackend::updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                                              const db_model::HashedId& pushkeyHashed,
                                              const db_model::HashedId& appIdHashed,
                                              const db_model::EncryptedBlob& encryptionKey)
{
    checkCommonPreconditions();
    A_27405.start("Schlüsselableitung - Alte Schlüssel löschen");
    static constexpr auto sql = R"(
UPDATE erp.app_registrations
SET encryption_key = $1
WHERE pushkey_hashed = $2 AND app_id_hashed = $3 AND kvnr_hashed = $4 ;
)";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "updateencryptionkey");
    const auto _ = transaction()
                       ->exec(sql, pqxx::params{encryptionKey.binarystring(), pushkeyHashed.binarystring(),
                                                appIdHashed.binarystring(), kvnrHashed.binarystring()})
                       .no_rows();
}
// GEMREQ-end A_27405

void MainPostgresBackend::deletePushKey(const db_model::HashedKvnr& hashedKvnr, const db_model::HashedId& hashedPushKey)
{
    checkCommonPreconditions();
    static constexpr auto sql = R"(
DELETE FROM erp.app_registrations
WHERE pushkey_hashed = $1 AND kvnr_hashed = $2;
)";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "deletepushkey");
    const auto _ =
        transaction()->exec(sql, pqxx::params{hashedPushKey.binarystring(), hashedKvnr.binarystring()}).no_rows();
}

PostgresConnection& exporter::MainPostgresBackend::threadConnection()
{
    static thread_local PostgresConnection connection{{PostgresConnection::defaultConnectParameters()}};
    return connection;
}
