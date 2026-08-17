/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/database/push/PushExporterDatabaseException.hxx"
#include "erp/database/push/PushExporterPostgresBackend.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/database/PostgresConnection.hxx"
#include "shared/database/PostgresConnectionParameters.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

PushExporterPostgresBackend::PushExporterPostgresBackend(PostgresConnection& connection)
    : mConnection(connection)
    , mTransaction(nullptr)
{
    try
    {
        connection.connectIfNeeded();
        mTransaction = connection.createTransaction(TransactionMode::transaction);
    }
    catch (const pqxx::broken_connection& exc)
    {
        TLOG(ERROR) << "Exporter DB not available.";
    }
}

void PushExporterPostgresBackend::healthCheck() const
{
    checkCommonPreconditions();
    constexpr auto query = "SELECT FROM erp_event.push_notification_event LIMIT 1";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck");
    TVLOG(2) << query;
    const auto _ = transaction()->exec(query);
}

void PushExporterPostgresBackend::commitTransaction()
{
    if (! mTransaction)
    {
        return;
    }

    push::erp::exporter::error::withDatabaseErrorHandling("committing database transaction.", [&]
    {
        TVLOG(1) << "committing transaction";
        mTransaction->commit();
        mTransaction.reset();
    });
}

bool PushExporterPostgresBackend::isCommitted() const
{
    return ! mTransaction;
}

void PushExporterPostgresBackend::closeConnection()
{
    if (! mTransaction)
    {
        connection().close();
        return;
    }
    TVLOG(1) << "closing connection to database";
    mTransaction.reset();
    connection().close();
    TVLOG(2) << "connection closed";
}

PostgresConnection& PushExporterPostgresBackend::connection() const
{
    return mConnection;
}

std::optional<DatabaseConnectionInfo> PushExporterPostgresBackend::getConnectionInfo() const
{
    return connection().getConnectionInfo();
}

void PushExporterPostgresBackend::checkCommonPreconditions() const
{
    Expect3(mTransaction, "Transaction already committed", std::logic_error);
}

const std::unique_ptr<pqxx::transaction_base>& PushExporterPostgresBackend::transaction() const
{
    return mTransaction;
}

PostgresConnection& PushExporterPostgresBackend::mainConnection()
{
    static thread_local PostgresConnection connection{{defaultConnectParameters()}};
    return connection;
}

PostgresConnectionParameters PushExporterPostgresBackend::defaultConnectParameters()
{
    const auto& configuration = Configuration::instance();
    return {
        .host = configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_HOST),
        .port = configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PORT),
        .user = configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USER),
        .password = configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PASSWORD),
        .dbname = configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_DATABASE),
        .connectTimeout =
            configuration.getStringValue(ConfigurationKey::POSTGRES_PUSHDB_CONNECT_TIMEOUT),
        .enableScramAuthentication =
            configuration.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_ENABLE_SCRAM_AUTHENTICATION),
        .tcpUserTimeoutMs =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TCP_USER_TIMEOUT_MS),
        .keepalivesIdleSec =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_IDLE_SEC),
        .keepalivesIntervalSec =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_INTERVAL_SEC),
        .keepalivesCountSec =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_COUNT),
        .targetSessionAttrs =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TARGET_SESSION_ATTRS),
        .useSsl = configuration.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USESSL),
        .serverRootCertPath =
            configuration.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_ROOT_CERTIFICATE_PATH),
        .sslCertificatePath =
            configuration.getOptionalStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_CERTIFICATE_PATH),
        .sslKeyPath = configuration.getOptionalStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_KEY_PATH),
    };
}

void PushExporterPostgresBackend::createPushEvent(const db_model::PushEventData& data)
{
    for (const auto& prescription : data.prescriptions())
    {
        createPushEvent(data, prescription);
    }
}

void PushExporterPostgresBackend::createPushEvent(const db_model::PushEventData& data, const model::PrescriptionId& prescriptionId) const
{
    if (! mTransaction)
    {
        return;
    }
    A_28124.start("Use only kvnr_hashed, prescription_id, prescription_type and channel_id");
    checkCommonPreconditions();
    constexpr std::string_view insert =
        "INSERT INTO erp_event.push_notification_event (kvnr_hashed, next_export, created, prescription_id, "
        "prescription_type, channel_id, notification_identifier, retry_count, state) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "createpushevent");
    TVLOG(2) << insert;
    const auto _ =
        transaction()
            ->exec(insert,
                   pqxx::params{
                       data.hashedKvnr().binarystring(), model::Timestamp::now().toXsDateTime(),
                       model::Timestamp::now().toXsDateTime(), prescriptionId.toDatabaseId(),
                       gsl::narrow<int16_t>(magic_enum::enum_integer(prescriptionId.type())),
                       model::to_string(data.channelId()), data.notificationIdentifier(), 0, "pending"})
            .no_rows();
    A_28124.finish();
}
