/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/database/PostgresConnection.hxx"
#include "shared/database/PostgresConnectString.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/PrescriptionType.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/TLog.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <pqxx/pqxx>

namespace
{
#define QUERY(name, query) const MedicationExporterPostgresBackend::QueryDefinition name = {#name, query};

QUERY(sqlProcessNextKvnr, R"(
UPDATE
  erp_event.kvnr
SET
  state = 'processing', next_export = NOW() + (5 * interval '1 minute')
WHERE
  kvnr_hashed = (
    SELECT
      kvnr_hashed
    FROM
      erp_event.kvnr
    WHERE
      next_export < NOW() AND state in ('processing', 'pending')
    ORDER BY
      next_export
LIMIT
  1
FOR UPDATE SKIP LOCKED
    )
RETURNING
  kvnr_hashed, EXTRACT(EPOCH FROM last_consent_check), assigned_epa, state, retry_count;
    )");

QUERY(sqlGetAllEventsForKvnr, R"(
SELECT
  id, prescription_id, prescription_type, task_key_blob_id, salt, kvnr, kvnr_hashed, state, usecase, doctor_identity,
  pharmacy_identity,
  EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on), healthcare_provider_prescription,
  medication_dispense_blob_id, medication_dispense_salt, medication_dispense_bundle, retry_count
FROM
  erp_event.task_event
WHERE
  kvnr_hashed = $1::bytea AND state = 'pending'
ORDER BY
  prescription_id ASC, last_modified ASC;
    )");
QUERY(sqlCheckDeadletter, R"(
SELECT
  COUNT(*)
FROM erp_event.task_event
WHERE
  kvnr_hashed = $1 AND prescription_id = $2 AND prescription_type = $3 AND state = 'deadLetterQueue';
    )");
QUERY(sqlMarkDeadletter, R"(
UPDATE
  erp_event.task_event
SET
  state = 'deadLetterQueue'
WHERE
  kvnr_hashed = $1 AND prescription_id = $2 AND prescription_type = $3 AND state != 'deadLetterQueue'
    )");
QUERY(sqlMarkFirstEventDeadletter, R"(
UPDATE
  erp_event.task_event
  SET state='deadLetterQueue'
WHERE id = (
        SELECT id FROM erp_event.task_event
        WHERE kvnr_hashed = $1 AND state = 'pending'
        ORDER BY prescription_id ASC, last_modified ASC LIMIT 1
    )
RETURNING kvnr, kvnr_hashed, prescription_id, prescription_type, usecase, EXTRACT(EPOCH FROM authored_on), task_key_blob_id, salt
)");
QUERY(sqlDeleteOneEventForFKvnr, R"(
DELETE FROM
  erp_event.task_event
WHERE
  kvnr_hashed = $1 AND id = $2;
    )");
QUERY(sqlDeleteAllEventsForKvnr, R"(
DELETE FROM
  erp_event.task_event
WHERE
  kvnr_hashed = $1::bytea;
    )");
QUERY(sqlSetProcessingDelay, R"(
UPDATE
  erp_event.kvnr
SET
  state = 'pending',
  retry_count = $1,
  next_export = NOW() + ($2 * interval '1 second')
WHERE
  kvnr_hashed = $3::bytea;
    )");
QUERY(sqlFinalizeKvnr, R"(
UPDATE
  erp_event.kvnr
SET
  state = 'processed',
  retry_count = 0,
  next_export = null,
  assigned_epa = $2,
  last_consent_check = NOW()
WHERE
  kvnr_hashed = $1::bytea AND state = 'processing';
    )");

QUERY(sqlHealthCheck, R"(
SELECT
  *
FROM
  erp_event.task_event
LIMIT
  1;
    )");

    // T-Rezept

QUERY(sqlProcessNextTRezeptEvent, R"(
UPDATE
  erp_event.trezept_event tre
SET
  state = 'processing', next_export = NOW() + (5 * interval '1 minute')
WHERE
  tre.id = (
    SELECT
      id
    FROM
      erp_event.trezept_event
    WHERE
      next_export < NOW() AND state in ('processing', 'pending')
    ORDER BY
      retry_count ASC, next_export ASC
    LIMIT 1
    FOR UPDATE SKIP LOCKED
  )
RETURNING
  id,
  prescription_id,
  prescription_type,
  task_key_blob_id,
  salt,
  kvnr,
  kvnr_hashed,
  state,
  usecase,
  doctor_identity,
  pharmacy_identity,
  EXTRACT(EPOCH FROM last_modified),
  EXTRACT(EPOCH FROM authored_on),
  healthcare_provider_prescription,
  medication_dispense_blob_id,
  medication_dispense_salt,
  medication_dispense_bundle,
  retry_count
)");

QUERY(sqlDeleteTRezeptEvent, R"(
DELETE FROM
  erp_event.trezept_event
WHERE
  id = $1;
)");

QUERY(sqlUpdateRetryTRezeptEvent, R"(
UPDATE
  erp_event.trezept_event
SET
  state = 'pending',
  retry_count = $1,
  next_export = NOW() + ($2 * interval '1 second')
WHERE
  id = $3;
)");

QUERY(sqlCheckDeadletterTRezeptEvents, R"(
SELECT
  COUNT(*)
FROM erp_event.trezept_event
WHERE
  id = $1 AND state = 'deadLetterQueue';
)");

QUERY(sqlMarkDeadletterTRezeptEvent, R"(
UPDATE
  erp_event.trezept_event
SET
  state = 'deadLetterQueue'
WHERE
  id = $1 AND state != 'deadLetterQueue'
)");
}

thread_local PostgresConnection MedicationExporterPostgresBackend::mConnection{defaultConnectString()};

MedicationExporterPostgresBackend::MedicationExporterPostgresBackend(TransactionMode mode)
    : CommonPostgresBackend(mConnection, mode)
{
}

std::string MedicationExporterPostgresBackend::defaultConnectString(void)
{
    const auto& cfg = Configuration::instance();
    return PostgresConnectString{
        .host = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_HOST),
        .port = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PORT),
        .user = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USER),
        .password = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_PASSWORD),
        .dbname = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_DATABASE),
        .connectTimeout = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_CONNECT_TIMEOUT_SECONDS),
        .enableScramAuthentication =
            cfg.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_ENABLE_SCRAM_AUTHENTICATION),
        .tcpUserTimeoutMs = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TCP_USER_TIMEOUT_MS),
        .keepalivesIdleSec = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_IDLE_SEC),
        .keepalivesIntervalSec =
            cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_INTERVAL_SEC),
        .keepalivesCountSec = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_KEEPALIVES_COUNT),
        .targetSessionAttrs = cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_TARGET_SESSION_ATTRS),
        .useSsl = cfg.getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_USESSL),
        .serverRootCertPath =
            cfg.getStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_ROOT_CERTIFICATE_PATH),
        .sslCertificatePath =
            cfg.getOptionalStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_CERTIFICATE_PATH),
        .sslKeyPath = cfg.getOptionalStringValue(ConfigurationKey::MEDICATION_EXPORTER_POSTGRES_SSL_KEY_PATH),
    };
}

PostgresConnection& MedicationExporterPostgresBackend::connection() const
{
    return mConnection;
}

std::optional<model::EventKvnr> MedicationExporterPostgresBackend::processNextKvnr()
{
    checkCommonPreconditions();
    TVLOG(2) << sqlProcessNextKvnr.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "processnextkvnr");

    const auto results = transaction()->exec(sqlProcessNextKvnr.query);
    TVLOG(2) << "got " << results.size() << " results";

    if (results.empty())
    {
        // No data available or kvnr already locked.
    }
    else if (results.size() == 1)
    {
        auto res = results.at(0);
        const auto kvnr_hashed = res.at(0).as<std::basic_string<std::byte>>();
        const auto last_consent_check =
            res.at(1).is_null() ? std::nullopt : std::make_optional(model::Timestamp(res.at(1).as<double>()));
        const auto assigned_epa = res.at(2).is_null() ? std::nullopt : std::make_optional(res.at(2).as<std::string>());
        const auto state = magic_enum::enum_cast<model::EventKvnr::State>(res.at(3).as<std::string>());
        const std::int32_t retryCount = res.at(4).is_null() ? 0 : res.at(4).as<std::int32_t>();

        model::EventKvnr model(kvnr_hashed, last_consent_check, assigned_epa, state.value(), retryCount);
        return model;
    }
    else
    {
        ErpFail(HttpStatus::InternalServerError, "A maximum of one result row expected");
    }

    return std::nullopt;
}

std::vector<db_model::TaskEvent>
MedicationExporterPostgresBackend::getAllEventsForKvnr(const model::EventKvnr& eventKvnr)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlGetAllEventsForKvnr.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "getalleventsforkvnr");

    if (not eventKvnr.kvnrHashed().empty())
    {
        const auto results = transaction()->exec(sqlGetAllEventsForKvnr.query, pqxx::params{eventKvnr.kvnrHashed()});
        TVLOG(2) << "got " << results.size() << " results";

        std::vector<db_model::TaskEvent> models{};
        TaskEventQueryIndexes idx;
        for (const auto& res : results)
        {
            Expect(res.size() == idx.total, "Invalid number of fields in result row: " + std::to_string(res.size()));
            auto prescription_type_opt = magic_enum::enum_cast<model::PrescriptionType>(
                map<uint8_t, int16_t>(res, idx.prescriptionType, "prescription_type is null"));
            Expect(prescription_type_opt.has_value(), "could not cast to PrescriptionType");


            const auto& prescriptionId = model::PrescriptionId::fromDatabaseId(
                *prescription_type_opt, map<int64_t>(res, idx.prescriptionId, "prescription_id is null"));

            models.emplace_back(
                map<db_model::TaskEvent::id_t, model::TaskEvent::id_t>(res, idx.id, "id is null"), prescriptionId,
                map<std::int16_t, std::int16_t>(res, idx.prescriptionType, "prescription type is null"),
                map<BlobId, int32_t>(res, idx.keyBlobId, "blob id is null"),
                map<db_model::Blob, db_model::postgres_bytea>(res, idx.salt, "salt is null"),
                map<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.kvnr, "kvnr is null"),
                map<db_model::HashedKvnr, db_model::postgres_bytea>(res, idx.kvnrHashed, "kvnr_hashed is null"),
                map<std::string, std::string>(res, idx.state, "state is null"),
                map<std::string, std::string>(res, idx.usecase, "use case is null"),
                map<model::Timestamp, double>(res, idx.lastModified, "last_modified is null"),
                map<model::Timestamp, double>(res, idx.authoredOn, "authored_on is null"),
                mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.healthcareProviderPrescription),
                mapOptional<BlobId, int32_t>(res, idx.medicationDispenseBundleBlobId),
                mapOptional<db_model::Blob, db_model::postgres_bytea>(res, idx.medicationDispenseBundleSalt),
                mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.medicationDispenseBundle),
                mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.doctorIdentity),
                mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.pharmacyIdentity),
                mapDefault<std::int32_t, std::int32_t>(res, idx.retryCount, 0)
                );
        }
        return models;
    }

    return {};
}

bool MedicationExporterPostgresBackend::isDeadLetter(const model::EventKvnr& kvnr,
                                                     const model::PrescriptionId& prescriptionId,
                                                     model::PrescriptionType prescriptionType)
{
    const QueryDefinition& sql = sqlCheckDeadletter;
    checkCommonPreconditions();
    TVLOG(2) << sql.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "sqlcheckdeadletter");

    const auto result = transaction()->exec(
        sql.query, pqxx::params{kvnr.kvnrHashed(), prescriptionId.toDatabaseId(),
                                static_cast<std::int16_t>(magic_enum::enum_integer(prescriptionType))});
    TVLOG(2) << "got " << result.size() << " results";
    std::int64_t count = 0;
    Expect(result.at(0, 0).to(count), "Could not retrieve count of deadletters as int64_t");
    return count > 0;
}

int MedicationExporterPostgresBackend::markDeadLetter(const model::EventKvnr& kvnr,
                                                      const model::PrescriptionId& prescriptionId,
                                                      model::PrescriptionType prescriptionType)
{
    const QueryDefinition& sql = sqlMarkDeadletter;
    checkCommonPreconditions();
    TVLOG(2) << sql.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "markcheckdeadletter");

    const auto result = transaction()->exec(
        sql.query, pqxx::params{kvnr.kvnrHashed(), prescriptionId.toDatabaseId(),
                                static_cast<std::int16_t>(magic_enum::enum_integer(prescriptionType))});
    return result.affected_rows();
}

std::optional<db_model::BareTaskEvent>
MedicationExporterPostgresBackend::markFirstEventDeadLetter(const model::EventKvnr& kvnr)
{
    const QueryDefinition& sql = sqlMarkFirstEventDeadletter;
    checkCommonPreconditions();
    TVLOG(2) << sql.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "markfirsteventdeadletter");

    const auto result = transaction()->exec(sql.query, pqxx::params{kvnr.kvnrHashed()});

    if (! result.empty())
    {
        Expect(result.size() == 1, "Bad result marking first event as deadletter");
        const auto& res = result[0];

        MarkFirsEventDeadletterQueryIndexes idx;
        Expect(res.size() == idx.total, "Bad result marking first event as deadletter");

        auto prescription_type_opt = magic_enum::enum_cast<model::PrescriptionType>(
            map<uint8_t, int16_t>(res, idx.prescriptionType, "prescription_type is null"));
        Expect(prescription_type_opt.has_value(), "could not cast to PrescriptionType");
        const auto& prescriptionId = model::PrescriptionId::fromDatabaseId(
            *prescription_type_opt, map<int64_t>(res, idx.prescriptionId, "prescription_id is null"));

        return std::make_optional(db_model::BareTaskEvent{
            map<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.kvnr, "kvnr is null"),
            map<db_model::HashedKvnr, db_model::postgres_bytea>(result[0], idx.kvnrHashed, "kvnr_hashed is null"),
            map<std::string, std::string>(res, idx.usecase, "use case is null"),
            prescriptionId,
            map<std::int16_t, std::int16_t>(res, idx.prescriptionType, "prescription type is null"),
            map<model::Timestamp, double>(res, idx.authoredOn, "authored_on is null"),
            map<BlobId, int32_t>(res, idx.keyBlobId, "blob id is null"),
            map<db_model::Blob, db_model::postgres_bytea>(res, idx.salt, "salt is null"),
        });
    }
    return std::nullopt;
}

void MedicationExporterPostgresBackend::deleteAllEventsForKvnr(const model::EventKvnr& kvnr)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlDeleteAllEventsForKvnr.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "deletealleventsforkvnr");

    Expect(not kvnr.kvnrHashed().empty(), "Kvnr missing");

    std::basic_string<std::byte> s{kvnr.kvnrHashed().data(), kvnr.kvnrHashed().size()};

    const auto results = transaction()->exec(sqlDeleteAllEventsForKvnr.query, pqxx::params{s});
    TVLOG(2) << "got " << results.size() << " results";
}

void MedicationExporterPostgresBackend::updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay, const model::EventKvnr& kvnr)
{
    const MedicationExporterPostgresBackend::QueryDefinition& sql = sqlSetProcessingDelay;
    checkCommonPreconditions();
    TVLOG(2) << sql.query;

    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "postponeprocessing24h");

    Expect(not kvnr.kvnrHashed().empty(), "Kvnr missing");

    const auto results = transaction()->exec(sql.query, pqxx::params{newRetry, delay.count(), kvnr.kvnrHashed()});
    TVLOG(2) << "affected " << results.affected_rows() << " rows";
}

void MedicationExporterPostgresBackend::deleteOneEventForKvnr(const model::EventKvnr& kvnr,
                                                              db_model::TaskEvent::id_t id)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlDeleteOneEventForFKvnr.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "sqldeleteoneeventforfkvnr");

    Expect(not kvnr.kvnrHashed().empty(), "Kvnr missing");

    std::basic_string<std::byte> s{kvnr.kvnrHashed().data(), kvnr.kvnrHashed().size()};

    const auto results = transaction()->exec(sqlDeleteOneEventForFKvnr.query, pqxx::params{s, id});
    TVLOG(2) << "got " << results.size() << " results";
}

void MedicationExporterPostgresBackend::finalizeKvnr(const model::EventKvnr& kvnr,
                                                     const std::string& assignedEpaPrefix) const
{
    checkCommonPreconditions();
    TVLOG(2) << sqlFinalizeKvnr.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "sqlfinalizekvnr");

    Expect(not kvnr.kvnrHashed().empty(), "Kvnr missing");

    std::basic_string<std::byte> s{kvnr.kvnrHashed().data(), kvnr.kvnrHashed().size()};

    const auto parts = String::split(assignedEpaPrefix, '.');
    Expect(not parts.empty(), "Invalid fqdn");
    const auto results = transaction()->exec(sqlFinalizeKvnr.query, pqxx::params{s, parts[0]});

    TVLOG(2) << "got " << results.size() << " results";
}

std::optional<db_model::TaskEvent> MedicationExporterPostgresBackend::processNextTRezeptEvent()
{
    checkCommonPreconditions();
    TVLOG(2) << sqlProcessNextTRezeptEvent.query;
    auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "processnexttrezeptevent");

    const auto results = transaction()->exec(sqlProcessNextTRezeptEvent.query);
    TVLOG(2) << "got " << results.size() << " results";

    if (results.empty())
    {
        return std::nullopt;
    }

    Expect(results.size() == 1, "A maximum of one result row expected, but got " + std::to_string(results.size()));

    auto res = results.at(0);

    TaskEventQueryIndexes idx;

    Expect(res.size() == idx.total, "Invalid number of fields in result row: " + std::to_string(res.size()));

    auto prescription_type_opt = magic_enum::enum_cast<model::PrescriptionType>(
        map<uint8_t, int16_t>(res, idx.prescriptionType, "prescription_type is null"));
    Expect(prescription_type_opt.has_value(), "could not cast to PrescriptionType");

    const auto& prescriptionId = model::PrescriptionId::fromDatabaseId(
        *prescription_type_opt, map<int64_t>(res, idx.prescriptionId, "prescription_id is null"));
    timerKeepAlive.keyValue(std::string{"prescription_id"}, prescriptionId.toString());
    db_model::TaskEvent dbModel(
        map<db_model::TaskEvent::id_t, model::TaskEvent::id_t>(res, idx.id, "id is null"),
        prescriptionId,
        map<std::int16_t, std::int16_t>(res, idx.prescriptionType, "prescription type is null"),
        map<BlobId, int32_t>(res, idx.keyBlobId, "blob id is null"),
        map<db_model::Blob, db_model::postgres_bytea>(res, idx.salt, "salt is null"),
        map<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.kvnr, "kvnr is null"),
        map<db_model::HashedKvnr, db_model::postgres_bytea>(res, idx.kvnrHashed, "kvnr_hashed is null"),
        map<std::string, std::string>(res, idx.state, "state is null"),
        map<std::string, std::string>(res, idx.usecase, "use case is null"),
        map<model::Timestamp, double>(res, idx.lastModified, "last_modified is null"),
        map<model::Timestamp, double>(res, idx.authoredOn, "authored_on is null"),
        mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.healthcareProviderPrescription),
        mapOptional<BlobId, int32_t>(res, idx.medicationDispenseBundleBlobId),
        mapOptional<db_model::Blob, db_model::postgres_bytea>(res, idx.medicationDispenseBundleSalt),
        mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.medicationDispenseBundle),
        mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.doctorIdentity),
        mapOptional<db_model::EncryptedBlob, db_model::postgres_bytea>(res, idx.pharmacyIdentity),
        mapDefault<std::int32_t, std::int32_t>(res, idx.retryCount, 0));

    return dbModel;
}

void MedicationExporterPostgresBackend::deleteTRezeptEvent(model::TRezeptEvent::id_t eventId)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlDeleteTRezeptEvent.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "sqldeletetrezeptevent");

    const auto results = transaction()->exec(sqlDeleteTRezeptEvent.query, pqxx::params{eventId});
    TVLOG(2) << "got " << results.size() << " results";
}

void MedicationExporterPostgresBackend::updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay, const model::TRezeptEvent& eventData)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlUpdateRetryTRezeptEvent.query;

    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "sqlupdateretrytrezeptevent");

    const auto results = transaction()->exec(sqlUpdateRetryTRezeptEvent.query, pqxx::params{newRetry, delay.count(), eventData.getId()});
    TVLOG(2) << "affected " << results.affected_rows() << " rows";
}

void MedicationExporterPostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << sqlHealthCheck.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck");
    const auto result = transaction()->exec(sqlHealthCheck.query);
    TVLOG(2) << "got " << result.size() << " results";
}

bool MedicationExporterPostgresBackend::isDeadLetter(const model::TRezeptEvent& eventData)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlCheckDeadletterTRezeptEvents.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "sqlcheckdeadlettertrezeptevents");
    const auto result = transaction()->exec(sqlCheckDeadletterTRezeptEvents.query, pqxx::params{eventData.getId()});
    TVLOG(2) << "got " << result.size() << " results";
    std::int64_t count = 0;
    Expect(result.at(0, 0).to(count), "Could not retrieve count of deadletters as int64_t");
    return count > 0;
}

int MedicationExporterPostgresBackend::markDeadLetter(const model::TRezeptEvent& eventData)
{
    checkCommonPreconditions();
    TVLOG(2) << sqlMarkDeadletterTRezeptEvent.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "sqlmarkdeadlettertrezeptevent");
    const auto result = transaction()->exec(sqlMarkDeadletterTRezeptEvent.query, pqxx::params{eventData.getId()});
    return result.affected_rows();
}
