/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/PostgresBackend.hxx"
#include "erp/database/ErpDatabaseModel.hxx"
#include "erp/model/Consent.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CMAC.hxx"
#include "shared/model/Binary.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/JsonLog.hxx"

#include <boost/algorithm/string.hpp>
#include <pqxx/pqxx>


namespace
{
#define QUERY(name, query) const QueryDefinition name = {# name, query};
    QUERY(retrieveCmac                        , "SELECT cmac FROM erp.vau_cmac WHERE valid_date = $1 AND cmac_type = $2")
    // try to insert the new value
    // if that fails return the old value
    // ON CONFLICT DO UPDATE requires at least on assignment, therefore we set cmac to its current value.
    QUERY(acquireCmac                         , "INSERT INTO erp.vau_cmac (valid_date, cmac_type, cmac) VALUES ($1, $2, $3) "
                                                "    ON CONFLICT(valid_date, cmac_type) DO UPDATE SET cmac=erp.vau_cmac.cmac "
                                                "    RETURNING cmac;")
    // the sent time is encoded in the ID.
    // It could be retrieved by erp.timestamp_from_suid(id) or erp.epoch_from_suuid(id)
    QUERY(insertCommunicationStatement,        "INSERT INTO erp.communication (id, message_type, sender, recipient, "
                                               "            received, prescription_id, prescription_type, sender_blob_id,"
                                               "            message_for_sender, recipient_blob_id, "
                                               "            message_for_recipient)"
                                               "     VALUES (erp.gen_suuid($1), $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)"
                                               "RETURNING id")
    QUERY(countRepresentativeCommunicationsStatement, "SELECT COUNT(*) FROM erp.communication "
                                                "    WHERE message_type = $1 AND prescription_id = $4 "
                                                "    AND prescription_type = $5 AND "
                                                "((sender = $2 AND recipient = $3) OR (sender = $3 AND recipient = $2))")
    QUERY(countCommunicationsByIdStatement, "SELECT COUNT(*) FROM erp.communication WHERE id = $1")

    // GEMREQ-start A_19520-01#query
    QUERY(retrieveCommunicationsStatement, R"--(
        WITH communication AS (
            SELECT c.id, c.received, c.sender, c.recipient, c.message_for_sender AS message,
                    c.sender_blob_id AS blob_id, sender_account.salt AS salt
                    FROM erp.communication c
                LEFT JOIN erp.account sender_account ON
                    sender_account.account_id = c.sender AND
                    sender_account.master_key_type = 2 AND
                    sender_account.blob_id = c.sender_blob_id
                WHERE sender = $1
            UNION
            SELECT c.id, c.received, c.sender, c.recipient, c.message_for_recipient AS message,
                   c.recipient_blob_id AS blob_id, recipient_account.salt AS salt
                FROM erp.communication c
                LEFT JOIN erp.account recipient_account ON
                    recipient_account.account_id = c.recipient AND
                    recipient_account.master_key_type = 2 AND
                    recipient_account.blob_id = c.recipient_blob_id
                WHERE recipient = $1
        )
        SELECT id, TRUNC(EXTRACT(EPOCH FROM received)) AS received, message, blob_id, salt FROM communication
            WHERE ($2::uuid IS NULL OR id = $2::uuid))--")
    // GEMREQ-end A_19520-01#query
    QUERY(countCommunicationsStatement, R"--(
        SELECT COUNT(*)
            FROM erp.communication
            WHERE
                (recipient = $1 OR sender = $1))--")

    QUERY(retrieveCommunicationIdsStatement,   "SELECT id FROM erp.communication WHERE recipient = $1")
    QUERY(deleteCommunicationStatement,        "DELETE FROM erp.communication WHERE id = $1::uuid AND sender = $2 "
                                               "RETURNING id")
    QUERY(updateCommunicationsRetrievedStatement, R"--(
        UPDATE erp.communication
            SET received = $2
            WHERE
                ((received IS NULL)
                AND (id = ANY($1::uuid[]))
                AND (recipient = $3))
        )--")
// GEMREQ-start A_19027-06#query-deleteCommunicationsForTask
    QUERY(deleteCommunicationsForTaskStatement,  "DELETE FROM erp.communication WHERE prescription_id = $1 AND prescription_type = $2")
// GEMREQ-end A_19027-06#query-deleteCommunicationsForTask

// GEMREQ-start A_22157#query-deleteChargeItemCommunicationsForKvnrStatement
    QUERY(deleteChargeItemCommunicationsForKvnrStatement,
          "DELETE FROM erp.communication WHERE (sender = $1 OR recipient = $1) "
          "AND (message_type = $2 OR message_type = $3)")
// GEMREQ-end A_22157#query-deleteChargeItemCommunicationsForKvnrStatement

// GEMREQ-start A_22117-01#query-deleteCommunicationsForChargeItemStatement
    QUERY(deleteCommunicationsForChargeItemStatement,
          "DELETE FROM erp.communication WHERE prescription_id = $1 AND prescription_type = $2 "
          "AND (message_type = $3 OR message_type = $4)")
// GEMREQ-end A_22117-01#query-deleteCommunicationsForChargeItemStatement

    QUERY(retrieveAuditEventDataStatement, R"--(
          SELECT id, erp.epoch_from_suuid(id) AS recorded, event_id, action, agent_type, observer, prescription_id, prescription_type, metadata, blob_id
          FROM erp.auditevent
          WHERE
              kvnr_hashed = $1
              AND ($2::uuid IS NULL OR id = $2::uuid)
              AND ($3::bigint IS NULL OR (prescription_id = $3::bigint AND prescription_type = $4::smallint)))--")

    QUERY(retrieveAllMedicationDispensesByKvnr, R"--(
        SELECT prescription_id, medication_dispense_bundle, medication_dispense_blob_id, medication_dispense_salt, prescription_type, is_pkv from erp.task_view
        WHERE kvnr_hashed = $1 AND ($2::bigint IS NULL OR (prescription_id = $2::bigint AND prescription_type = $3::smallint))
            AND medication_dispense_bundle IS NOT NULL
        )--")

// GEMREQ-start A_19115-01#query
    QUERY(retrieveAllTasksByKvnr, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id, prescription_type,
            eu_redeemable_patient, eu_redeemable, is_pkv FROM erp.task_view
        WHERE kvnr_hashed = $1
        )--")
// GEMREQ-end A_19115-01#query

// GEMREQ-start A_23452-02#query
QUERY(retrieveAllTasksByKvnrWithAccessCode, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id, prescription_type,
            access_code, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM erp.task_view
        WHERE kvnr_hashed = $1
        )--")
// GEMREQ-end A_23452-02#query

    QUERY(countAllTasksByKvnr, R"--( SELECT COUNT(*) FROM erp.task_view WHERE kvnr_hashed = $1)--")

    QUERY(retrieveAllTasksForEuStatement, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id, prescription_type, healthcare_provider_prescription, owner, eu_redeemable_patient, eu_redeemable FROM erp.task_view
        WHERE kvnr_hashed = $1 AND eu_redeemable = true AND eu_redeemable_patient = true
    )--");

    QUERY(countAllTasksForEuStatement, R"--( SELECT COUNT(*) FROM erp.task_view WHERE kvnr_hashed = $1 AND eu_redeemable = true AND eu_redeemable_patient = true)--")

    QUERY(storeConsent, R"--(
        INSERT INTO erp.consent (kvnr_hashed, date_time, category) VALUES ($1, $2, $3)
    )--")

    QUERY(retrieveAllConsents, R"--(
        SELECT EXTRACT(EPOCH FROM date_time) as date_time, category FROM erp.consent WHERE kvnr_hashed = $1
    )--")

// GEMREQ-start A_22158#query
    QUERY(clearConsent, R"--(
        DELETE FROM erp.consent WHERE kvnr_hashed = $1 AND category = $2
    )--")
// GEMREQ-end A_22158#query

    QUERY(healthCheckQuery, "SELECT FROM erp.task LIMIT 1")

    QUERY(existsCountryCodeQuery,
          R"--(SELECT EXISTS ( SELECT 1 FROM erp.eu_country_codes WHERE iso_3166_alpha_2 = $1 ))--")

    QUERY(deleteEuAccessPermissionQuery, R"--(DELETE FROM erp.eu_access_permission where kvnr_hashed = $1)--")
    QUERY(createEuAccessPermissionQuery,
          R"--(INSERT INTO erp.eu_access_permission (kvnr_hashed, country_code, access_code, blob_id, salt, expires) VALUES ($1, $2, $3, $4, $5, $6))--")
    QUERY(retrieveEuAccessPermissionQuery,
          R"--(SELECT country_code, access_code, blob_id, salt, EXTRACT(EPOCH FROM expires) as expires FROM erp.eu_access_permission WHERE kvnr_hashed = $1)--")

#undef QUERY


}  // anonymous namespace


PostgresBackend::PostgresBackend(PostgresConnection& connection)
    : CommonPostgresBackend(connection)
    , mConnection{connection}
    , mBackendTask(model::PrescriptionType::apothekenpflichigeArzneimittel)
    , mBackendTask162(model::PrescriptionType::digitaleGesundheitsanwendungen)
    , mBackendTask166(model::PrescriptionType::tRezept)
    , mBackendTask169(model::PrescriptionType::direkteZuweisung)
    , mBackendTask200(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv)
    , mBackendTask209(model::PrescriptionType::direkteZuweisungPkv)
{
}

std::vector<db_model::MedicationDispense>
PostgresBackend::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnrHashed,
                                                const std::optional<model::PrescriptionId>& prescriptionId,
                                                const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrieveallmedicationdispenses");

    std::string query = retrieveAllMedicationDispensesByKvnr.query;
    if (search.has_value())
    {
        query.append(search->getSqlExpression(transaction()->conn(), "                ", true));
    }
    TVLOG(2) << query;

    const pqxx::result results = transaction()->exec(
        query,
        {kvnrHashed.binarystring(), prescriptionId ? prescriptionId->toDatabaseId() : std::optional<std::int64_t>{},
         prescriptionId ? static_cast<int16_t>(magic_enum::enum_integer(prescriptionId->type()))
                        : std::optional<std::int16_t>{}});

    TVLOG(2) << "got " << results.size() << " results";

    if (prescriptionId.has_value())
    {
        ErpExpect(results.size() <= 1, HttpStatus::InternalServerError, "A maximum of one result row expected");
    }

    std::vector<db_model::MedicationDispense> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(results.size()));
    for (const auto& res : results)
    {
        Expect(res.size() == 6, "Too few fields in result row");
        Expect(not res.at(0).is_null(), "prescription_id is null.");
        Expect(not res.at(1).is_null(), "medication_dispense_bundle is null.");
        Expect(not res.at(2).is_null(), "medication_dispense_blob_id is null.");
        Expect(not res.at(3).is_null(), "medication_dispense_salt is null.");
        Expect(not res.at(4).is_null(), "prescription_type is null.");
        Expect(not res.at(5).is_null(), "is_pkv is null.");
        const auto prescription_type_opt =
            magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<uint8_t>(res.at(4).as<int16_t>()));
        Expect(prescription_type_opt.has_value(), "could not cast to prescription_type");
        auto id = model::PrescriptionId::fromDatabaseId(prescription_type_opt.value(), res[0].as<int64_t>());
        resultSet.emplace_back(id, db_model::EncryptedBlob{res[1].as<db_model::postgres_bytea>()},
                               gsl::narrow<BlobId>(res[2].as<int32_t>()),
                               db_model::Blob{res[3].as<db_model::postgres_bytea>()});
    }

    return resultSet;
}

CmacKey PostgresBackend::acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource)
{
    checkCommonPreconditions();
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "acquirecmac");

    std::ostringstream validDateStrm;
    validDateStrm << date::year_month_day{validDate};
    { // scope
        TVLOG(2) << ::retrieveCmac.query;
        auto selectResult =
            transaction()->exec(retrieveCmac.query, pqxx::params{validDateStrm.str(), magic_enum::enum_name(cmacType)});
        TVLOG(2) << "got " << selectResult.size() << " results";
        if (!selectResult.empty())
        {
            Expect(selectResult.size() == 1, "Expected only one CMAC.");
            TLOG(INFO) << "CMAC for " << toString(cmacType) << " loaded from Database";
            auto cmac = selectResult.front().at(0).as<db_model::postgres_bytea>();
            return CmacKey::fromBin({reinterpret_cast<char*>(cmac.data()), cmac.size()});
        }
    }
    const auto& newKey = CmacKey::randomKey(randomSource);
    const db_model::postgres_bytea_view newKeyBin(reinterpret_cast<const std::byte*>(newKey.data()), newKey.size());
    { // scope
        TVLOG(2) << ::acquireCmac.query;
        auto acquireResult = transaction()->exec(
            ::acquireCmac.query, pqxx::params{validDateStrm.str(), magic_enum::enum_name(cmacType), newKeyBin});
        TVLOG(2) << "got " << acquireResult.size() << " results";
        Expect(acquireResult.size() == 1, "Expected exactly one CMAC.");
        TLOG(INFO) << "New CMAC for " << toString(cmacType) << " created";
        auto cmac = acquireResult.front().at(0).as<db_model::postgres_bytea>();
        return CmacKey::fromBin({reinterpret_cast<char*>(cmac.data()), cmac.size()});
    }
}

std::tuple<model::PrescriptionId, model::Timestamp> PostgresBackend::createTask(model::PrescriptionType prescriptionType,
                                                                                model::Task::Status taskStatus,
                                                                                const model::Timestamp& lastUpdated,
                                                                                const model::Timestamp& created,
                                                                                const model::Timestamp& lastStatusUpdate)
{
    checkCommonPreconditions();
    return getTaskBackend(prescriptionType)
        .createTask(*transaction(), taskStatus, lastUpdated, created, lastStatusUpdate);
}

void PostgresBackend::updateTask(const model::PrescriptionId& taskId,
                                  const db_model::EncryptedBlob& accessCode,
                                  uint32_t blobId,
                                  const db_model::Blob& salt)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type()).updateTask(*transaction(), taskId, accessCode, blobId, salt);
}

std::tuple<BlobId, db_model::Blob, model::Timestamp>
PostgresBackend::getTaskKeyData(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).getTaskKeyData(*transaction(), taskId);
}

void PostgresBackend::updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                                 model::Task::Status status,
                                                 const model::Timestamp& lastModifiedDate,
                                                 const std::optional<db_model::EncryptedBlob>& secret,
                                                 const std::optional<db_model::EncryptedBlob>& owner,
                                                 const model::Timestamp& lastStatusUpdate)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskStatusAndSecret(*transaction(), taskId, status, lastModifiedDate, secret, owner, lastStatusUpdate);
}

void PostgresBackend::activateTask(const model::PrescriptionId& taskId, const db_model::EncryptedBlob& encryptedKvnr,
                                   const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                                   const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                                   const model::Timestamp& acceptDate,
                                   const db_model::EncryptedBlob& healthCareProviderPrescription,
                                   const db_model::EncryptedBlob& doctorIdentity,
                                   const model::Timestamp& lastStatusUpdate,
                                   bool euRedeemable, bool isPkv)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .activateTask(*transaction(), taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate,
                      acceptDate, healthCareProviderPrescription, doctorIdentity, lastStatusUpdate, euRedeemable, isPkv);
}

void PostgresBackend::updateTaskReceipt(const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
                                        const model::Timestamp& lastModified, const db_model::EncryptedBlob& receipt,
                                        const db_model::EncryptedBlob& pharmacyIdentity,
                                        const model::Timestamp& lastStatusUpdate)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskReceipt(*transaction(), taskId, taskStatus, lastModified, receipt, pharmacyIdentity,
                           lastStatusUpdate);
}

void PostgresBackend::updateTaskMedicationDispense(
    const model::PrescriptionId& taskId,
    const model::Timestamp& lastModified,
    const model::Timestamp& lastMedicationDispense,
    const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId,
    const db_model::HashedTelematikId& telematikId,
    const model::Timestamp& whenHandedOver,
    const std::optional<model::Timestamp>& whenPrepared,
    const db_model::Blob& medicationDispenseSalt,
    const std::optional<model::Task::Status>& taskStatus,
    const db_model::EncryptedBlob& owner)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskMedicationDispense(*transaction(), taskId, lastModified, lastMedicationDispense, medicationDispense,
                                      medicationDispenseBlobId, telematikId, whenHandedOver, whenPrepared,
                                      medicationDispenseSalt, taskStatus, owner);
}

void PostgresBackend::updateTaskMedicationDispenseReceipt(
    const model::PrescriptionId& taskId, const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
    const db_model::EncryptedBlob& medicationDispense, BlobId medicationDispenseBlobId,
    const db_model::HashedTelematikId& telematikId, const model::Timestamp& whenHandedOver,
    const std::optional<model::Timestamp>& whenPrepared, const db_model::EncryptedBlob& receipt,
    const model::Timestamp& lastMedicationDispense, const db_model::Blob& medicationDispenseSalt,
    const db_model::EncryptedBlob& pharmacyIdentity,
    const model::Timestamp& lastStatusUpdate,
    const db_model::EncryptedBlob& owner)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskMedicationDispenseReceipt(*transaction(), taskId, taskStatus, lastModified, medicationDispense,
                                             medicationDispenseBlobId, telematikId, whenHandedOver, whenPrepared,
                                             receipt, lastMedicationDispense, medicationDispenseSalt, pharmacyIdentity,
                                             lastStatusUpdate, owner);
}

void PostgresBackend::updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId,
                                                         const model::Timestamp& lastModified)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type()).updateTaskDeleteMedicationDispense(*transaction(), taskId, lastModified);
}

// GEMREQ-start A_19027-06#query-call-updateTaskClearPersonalData
void PostgresBackend::updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                                   model::Task::Status taskStatus,
                                                   const model::Timestamp& lastModified,
                                                   const model::Timestamp& lastStatusUpdate)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskClearPersonalData(*transaction(), taskId, taskStatus, lastModified, lastStatusUpdate);
}
// GEMREQ-end A_19027-06#query-call-updateTaskClearPersonalData

void PostgresBackend::updateTaskEuRedeemableByPatient(const model::PrescriptionId& taskId,
                                                      bool redeemable,
                                                      const model::Timestamp& lastModified)
{
    checkCommonPreconditions();
    getTaskBackend(taskId.type())
        .updateTaskEuRedeemableByPatient(*transaction(), taskId, redeemable, lastModified);
}

std::vector<db_model::AuditData> PostgresBackend::retrieveAuditEventData(
        const db_model::HashedKvnr& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrieveauditeventdata");

    std::string query = retrieveAuditEventDataStatement.query;

    // Append an expression to the query for the search, sort and paging arguments, if there are any.
    A_19398.start("Add SQL expressions for searching, sorting and paging");
    if (search.has_value())
    {
        query += search->getSqlExpression(mConnection, "                ");
    }
    A_19398.finish();

    std::optional<std::string> idStr = id.has_value() ? id->toString() : std::optional<std::string>{};

    std::optional<int64_t> prescriptionIdNum;
    std::optional<int16_t> prescriptionTypeNum;
    if (prescriptionId.has_value())
    {
        prescriptionIdNum = prescriptionId->toDatabaseId();
        prescriptionTypeNum = static_cast<int16_t>(magic_enum::enum_integer(prescriptionId->type()));
    }

    // Run the query. Arguments that are passed on to exec are quoted and escaped automatically.
    A_19396.start("Filter audit events by Kvnr as part of the query");
    TVLOG(2) << query;
    const pqxx::result results =
        transaction()->exec(query, pqxx::params{kvnr.binarystring(), idStr, prescriptionIdNum, prescriptionTypeNum});
    A_19396.finish();

    TVLOG(2) << "got " << results.size() << " results";

    std::vector<db_model::AuditData> resultSet;
    for (const auto& res : results)
    {
        Expect(res.size() == 10, "Query result has unexpected number of values");
        const auto resultId = res.at(0).as<std::string>();
        const auto eventId = res.at(2).as<std::int16_t>();
        Expect(eventId >= 0 && eventId <= static_cast<std::int16_t>(model::AuditEventId::MAX), "Invalid AuditEvent id");
        const auto actionStr = res.at(3).as<std::string>();
        const auto agentType = magic_enum::enum_cast<model::AuditEvent::AgentType>(res.at(4).as<std::int16_t>());
        Expect(agentType, "Invalid AgentType");
        Expect(actionStr.size() == 1, "AuditEvent action must be single character");
        const auto deviceId = res.at(5).as<std::int16_t>();
        std::optional<model::PrescriptionId> prescriptionIdRes;
        if (! res.at(6).is_null() && ! res.at(7).is_null())
        {
            const auto prescriptionType =
                magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<uint8_t>(res.at(7).as<int16_t>()));
            Expect(prescriptionType.has_value(), "PrescriptionType could not be retrieved from audit entry");
            prescriptionIdRes.emplace(
                model::PrescriptionId::fromDatabaseId(*prescriptionType, res.at(6).as<std::int64_t>()));
        }

        std::optional<db_model::EncryptedBlob> metaData;
        if(! res.at(8).is_null())
        {
             metaData = db_model::EncryptedBlob(res.at(8).as<db_model::postgres_bytea>());
        }
        const auto blobId = res.at(9).is_null() ? std::optional<BlobId>() : res.at(9).as<int>();

        db_model::AuditData auditData(
            *agentType, static_cast<model::AuditEventId>(eventId), metaData,
            model::AuditEvent::ActionNamesReverse.at(actionStr), kvnr, deviceId, prescriptionIdRes, blobId);
        auditData.id = resultId;
        auditData.recorded = model::Timestamp(res.at(1).as<double>());
        resultSet.push_back(std::move(auditData));
    }

    return resultSet;
}

std::optional<db_model::Task> PostgresBackend::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskForUpdate(*transaction(), taskId);
}

::std::optional<::db_model::Task>
PostgresBackend::retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskForUpdateAndPrescription(*transaction(), taskId);
}

std::optional<db_model::Task> PostgresBackend::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskAndReceipt(*transaction(), taskId);
}

std::optional<db_model::Task>
PostgresBackend::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskAndPrescription(*transaction(), taskId);
}

std::optional<db_model::Task>
PostgresBackend::retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskWithSecretAndPrescription(*transaction(), taskId);
}

std::optional<db_model::Task>
PostgresBackend::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    return getTaskBackend(taskId.type()).retrieveTaskAndPrescriptionAndReceipt(*transaction(), taskId);
}

std::vector<db_model::Task> PostgresBackend::retrieveAllTasksForPatient (
    const db_model::HashedKvnr& kvnrHashed,
    const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievealltasksforpatient");

    std::string query = retrieveAllTasksByKvnr.query;
    if (search.has_value())
    {
        A_19569_03.start("Add search parameter to query");
        query.append(search->getSqlExpression(transaction()->conn(), "                "));
        A_19569_03.finish();
    }
    TVLOG(2) << query;

    const pqxx::result results = transaction()->exec(query, pqxx::params{kvnrHashed.binarystring()});

    TVLOG(2) << "got " << results.size() << " results";

    if (!results.empty())
    {
        Expect(results.front().size() == 14,
               "Invalid number of fields in result row: " + std::to_string(results.front().size()));
        return PostgresBackendTask::tasksFromQueryResult(results, std::nullopt);
    }
    return {};
}

std::vector<db_model::Task>
PostgresBackend::retrieveAllEgkRedeemableTasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                                             const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    auto prescriptionTypesView{magic_enum::enum_values<model::PrescriptionType>() |
                               std::views::filter(model::isEgkRedeemable)};
    const std::vector<model::PrescriptionType> prescriptionTypes{prescriptionTypesView.begin(),
                                                                 prescriptionTypesView.end()};

    std::string completeQuery = retrieveAllTasksByKvnrWithAccessCode.query;
    appendWherePrescriptionTypeIn(completeQuery, prescriptionTypes);
    if (search.has_value())
    {
        completeQuery.append(search->getSqlExpression(transaction()->conn(), "                "));
    }
    TVLOG(2) << completeQuery;

    const auto transactionTimer =
        ::DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "retrievealltasksbykvnrwithaccesscode");

    const auto results = transaction()->exec(completeQuery, pqxx::params{kvnrHashed.binarystring()});

    TVLOG(2) << "got " << results.size() << " results";
    if (! results.empty())
    {
        Expect(results.front().size() == 15,
               "Invalid number of fields in result row: " + std::to_string(results.front().size()));
    }
    return PostgresBackendTask::tasksFromQueryResult(results, std::nullopt);
}

uint64_t PostgresBackend::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                                  const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "countalltasksforpatient");
    return executeCountQuery(*transaction(), countAllTasksByKvnr.query, kvnr, search, "tasks");
}

uint64_t PostgresBackend::countAllEgkRedeemableTasks(const db_model::HashedKvnr& kvnr,
                                                     const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    auto prescriptionTypesView{magic_enum::enum_values<model::PrescriptionType>() |
                               std::views::filter(model::isEgkRedeemable)};
    const std::vector<model::PrescriptionType> prescriptionTypes{prescriptionTypesView.begin(),
                                                                 prescriptionTypesView.end()};
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "countall160tasksbykvnr");
    return executeCountQuery(*transaction(), countAllTasksByKvnr.query, kvnr, search, "tasks", prescriptionTypes);
}

std::vector<db_model::Task> PostgresBackend::retrieveAllTasksForEu(const db_model::HashedKvnr& kvnrHashed,
                                                                   const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto durationGuard =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "retrievealltasksforeu");

    std::string query = retrieveAllTasksForEuStatement.query;
    // GEMREQ-start A_27582#prescriptionIdsFilterSQL
    if (search.has_value())
    {
        query.append(search->getSqlExpression(transaction()->conn(), "                "));
    }
    // GEMREQ-end A_27582#prescriptionIdsFilterSQL
    query.append(" FOR UPDATE");
    TVLOG(2) << query;

    const pqxx::result results = transaction()->exec(query, {kvnrHashed.binarystring()});

    TVLOG(2) << "got " << results.size() << " results";
    std::vector<db_model::Task> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(results.size()));

    for (const auto& res : results)
    {
        Expect(res.size() == 15, "Invalid number of fields in result row: " + std::to_string(res.size()));
        Expect(! res.at(10).is_null(), "prescription_type is null");
        auto prescription_type_opt =
            magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<uint8_t>(res.at(10).as<int16_t>()));
        Expect(prescription_type_opt.has_value(), "could not cast to PrescriptionType");
        resultSet.emplace_back(
            PostgresBackendTask::taskFromQueryResultRow(res, prescription_type_opt.value()));
    }
    return resultSet;
}

uint64_t PostgresBackend::countAllTasksForEu(const db_model::HashedKvnr& kvnr,
                                             const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto durationGuard =
    DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "countalltasksforeu");
    return executeCountQuery(*transaction(), countAllTasksForEuStatement.query, kvnr, search, "EU tasks");
}

std::optional<Uuid> PostgresBackend::insertCommunication(const model::PrescriptionId& prescriptionId,
                                                         const model::Timestamp& timeSent,
                                                         const model::Communication::MessageType messageType,
                                                         const db_model::HashedId& sender,
                                                         const db_model::HashedId& recipient,
                                                         BlobId senderBlobId,
                                                         const db_model::EncryptedBlob& messageForSender,
                                                         BlobId recipientBlobId,
                                                         const db_model::EncryptedBlob& messageForRecipient)
{
    checkCommonPreconditions();
    TVLOG(2) << insertCommunicationStatement.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "insertcommunication");

    const pqxx::result result = transaction()->exec(
        insertCommunicationStatement.query,
        pqxx::params{timeSent.toXsDateTime(), static_cast<int>(model::Communication::messageTypeToInt(messageType)),
                     sender.binarystring(), recipient.binarystring(), std::nullopt, prescriptionId.toDatabaseId(),
                     static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())),
                     gsl::narrow<int32_t>(senderBlobId), messageForSender.binarystring(),
                     gsl::narrow<int32_t>(recipientBlobId), messageForRecipient.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        std::string sId;
        Expect(result.front().at(0).to(sId), "Could not retrieve communication Id as int64");
        std::optional<Uuid> id{sId};
        return id;
    }
    return {};
}

uint64_t PostgresBackend::countRepresentativeCommunications(
    const db_model::HashedKvnr& insurantA,
    const db_model::HashedKvnr& insurantB,
    const model::PrescriptionId& prescriptionId)
{
    checkCommonPreconditions();
    TVLOG(2) << countRepresentativeCommunicationsStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "countrepresentativecommunications");

    const auto result = transaction()->exec(
        countRepresentativeCommunicationsStatement.query,
        pqxx::params{
            /* $1 */
            static_cast<int>(model::Communication::messageTypeToInt(model::Communication::MessageType::Representative)),
            /* $2 */ insurantA.binarystring(),
            /* $3 */ insurantB.binarystring(),
            /* $4 */ prescriptionId.toDatabaseId(),
            /* $5 */ static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type()))});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of communications as int64");

    return gsl::narrow<uint64_t>(count);
}

bool PostgresBackend::existCommunication(const Uuid& communicationId)
{
    checkCommonPreconditions();
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "existcommunication");

    const pqxx::result result =
        transaction()->exec(countCommunicationsByIdStatement.query, pqxx::params{communicationId.toString()});

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of communications as int64");
    Expect(count <= 1, "More than one row for communication id in database");

    return (count > 0);
}

std::vector<db_model::Communication> PostgresBackend::retrieveCommunications (
    const db_model::HashedId& user,
    const std::optional<Uuid>& communicationId,
    const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievecommunications");

    // We don't use a prepared statement for this query because search arguments lead to dynamically generated
    // WHERE and SORT clauses. Paging can add LIMIT and OFFSET expressions.
    A_19520_02.start("filtering by user as either sender or recipient is done by the constant part of the query");
    std::string query = retrieveCommunicationsStatement.query;
    A_19520_02.finish();

    // Append a an expression to the query for the search, sort and paging arguments, if there are any.
    A_19534.start("add SQL expressions for searching, sorting and paging");
    if (search.has_value())
        query += search->getSqlExpression(mConnection, "                ");
    A_19534.finish();

    std::optional<std::string> communicationIdStr =
        communicationId ? communicationId->toString() : std::optional<std::string>{};

    // Run the query. Arguments that are passed on to exec() are quoted and escaped automatically.
    TVLOG(2) << query;
    const pqxx::result results = transaction()->exec(query, pqxx::params{user.binarystring(), communicationIdStr});

    TVLOG(2) << "got " << results.size() << " results";

    std::vector<db_model::Communication> communications;
    communications.reserve(gsl::narrow<size_t>(results.size()));
    for (const auto& item : results)
    {
        ErpExpect(item.size() == 5, HttpStatus::InternalServerError, "query result has unexpected number of values");
        auto& newComm = communications.emplace_back();
        newComm.id = Uuid(item[0].as<std::string>());
        if (not item[1].is_null())
        {
            newComm.received = model::Timestamp{item[1].as<double>()};
        }
        newComm.communication = db_model::EncryptedBlob{item[2].as<db_model::postgres_bytea>()};
        newComm.blobId = gsl::narrow<BlobId>(item[3].as<int>());
        newComm.salt = db_model::Blob{item[4].as<db_model::postgres_bytea>()};
    }

    return communications;
}

uint64_t PostgresBackend::countCommunications(
    const db_model::HashedId& user,
    const std::optional<UrlArguments>& search)
{
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "countcommunications");
    return executeCountQuery(*transaction(), countCommunicationsStatement.query, user, search, "communications");
}


std::vector<Uuid> PostgresBackend::retrieveCommunicationIds (const db_model::HashedId& recipient)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveCommunicationIdsStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievecommunicationids");

    const pqxx::result results =
        transaction()->exec(retrieveCommunicationIdsStatement.query, pqxx::params{recipient.binarystring()});

    TVLOG(2) << "got " << results.size() << " results";

    std::vector<Uuid> ids;
    ids.reserve(gsl::narrow<size_t>(results.size()));
    for (const auto& item : results)
        ids.emplace_back(item.at(0).as<std::string>());

    return ids;
}

std::optional<Uuid> PostgresBackend::deleteCommunication(
    const Uuid& communicationId,
    const db_model::HashedId& sender)
{
    checkCommonPreconditions();
    TVLOG(2) << deleteCommunicationStatement.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "deletecommunication");

    const pqxx::result result = transaction()->exec(deleteCommunicationStatement.query,
                                                    pqxx::params{communicationId.toString(), sender.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() <= 1, "Expecting either no or one row as result.");
    if (result.size() == 1)
    {
        Expect(result.front().size() == 1, "Expecting one column as result containing id.");
        return Uuid(result.front().at(0).as<std::string>());
    }
    return std::nullopt;
}

void PostgresBackend::markCommunicationsAsRetrieved (
    const std::vector<Uuid>& communicationIds,
    const model::Timestamp& retrieved,
    const db_model::HashedId& recipient)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "markcommunicationsasretrieved");

    std::vector<std::string> communicationIdStrings;
    communicationIdStrings.reserve(communicationIds.size());
    for (const auto& uuid : communicationIds)
    {
        communicationIdStrings.emplace_back(uuid.toString());
    }

    TVLOG(2) << updateCommunicationsRetrievedStatement.query;
    const auto result =
        transaction()->exec(updateCommunicationsRetrievedStatement.query,
                            pqxx::params{communicationIdStrings, retrieved.toXsDateTime(), recipient.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";
}

// GEMREQ-start A_19027-06#query-call-deleteCommunicationsForTask
void PostgresBackend::deleteCommunicationsForTask (const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::deleteCommunicationsForTaskStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "deletecommunicationsfortask");

    const auto result = transaction()->exec(
        deleteCommunicationsForTaskStatement.query,
        pqxx::params{taskId.toDatabaseId(), static_cast<int16_t>(magic_enum::enum_integer(taskId.type()))});
    TVLOG(2) << "got " << result.size() << " results";
}
// GEMREQ-end A_19027-06#query-call-deleteCommunicationsForTask

void PostgresBackend::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime, db_model::ConsentCategory category)
{
    checkCommonPreconditions();
    TVLOG(2) << ::storeConsent.query;
    const auto durationConsumer = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                         "storeconsent");
    const auto& result = transaction()->exec(::storeConsent.query, {kvnr.binarystring(), creationTime.toXsDateTime(),
                       magic_enum::enum_name(category)}).no_rows();
    Expect(result.empty(), "Unexpected rows returned from storeConsent statement");
}

std::vector<db_model::Consent> PostgresBackend::retrieveAllConsents(const db_model::HashedKvnr& kvnr,
                                                                    const UrlArguments& searchArguments)
{
    checkCommonPreconditions();
    auto query = ::retrieveAllConsents.query;
    query.append(searchArguments.getSqlExpression(mConnection, "                "));
    TVLOG(0) << query;
    const auto durationConsumer = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                          "retrieveallconsents");
    const auto& result = transaction()->exec(query, {kvnr.binarystring()});
    std::vector<db_model::Consent> consents;
    consents.reserve(gsl::narrow<size_t>(result.size()));
    for (const auto& row : result)
    {
        Expect(row.size() == 2, "Expected exactly two columns.");
        auto category = magic_enum::enum_cast<db_model::ConsentCategory>(row[1].as<std::string>());
        Expect(category.has_value(), "Could not convert consent category to enum.");
        consents.emplace_back(*category, model::Timestamp{row[0].as<double>()});
    }
    return consents;
}

// GEMREQ-start A_22158#query-call
bool PostgresBackend::clearConsent(const db_model::HashedKvnr& kvnr, db_model::ConsentCategory category)
{
    checkCommonPreconditions();
    TVLOG(2) << ::clearConsent.query;
    const auto durationConsumer = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                          "clearconsent");
    auto result =
        transaction()->exec(::clearConsent.query, {kvnr.binarystring(), magic_enum::enum_name(category)}).no_rows();
    return result.affected_rows() > 0;
}
// GEMREQ-end A_22158#query-call

void PostgresBackend::storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr)
{
    checkCommonPreconditions();
    mBackendChargeItem.storeChargeInformation(*transaction(), chargeItem, kvnr);
}

void PostgresBackend::updateChargeInformation(const ::db_model::ChargeItem& chargeItem)
{
    checkCommonPreconditions();
    mBackendChargeItem.updateChargeInformation(*transaction(), chargeItem);
}

// GEMREQ-start A_22119#query-call
::std::vector<db_model::ChargeItem>
PostgresBackend::retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& kvnr,
                                                   const ::std::optional<UrlArguments>& search) const
{
    checkCommonPreconditions();
    return mBackendChargeItem.retrieveAllChargeItemsForInsurant(*transaction(), kvnr, search);
}
// GEMREQ-end A_22119#query-call

::db_model::ChargeItem PostgresBackend::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    checkCommonPreconditions();
    return mBackendChargeItem.retrieveChargeInformation(*transaction(), id);
}

::db_model::ChargeItem PostgresBackend::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    checkCommonPreconditions();
    return mBackendChargeItem.retrieveChargeInformationForUpdate(*transaction(), id);
}

// GEMREQ-start A_22117-01#query-call-deleteChargeInformation
void PostgresBackend::deleteChargeInformation(const model::PrescriptionId& id)
{
    checkCommonPreconditions();
    mBackendChargeItem.deleteChargeInformation(*transaction(), id);
}
// GEMREQ-end A_22117-01#query-call-deleteChargeInformation

// GEMREQ-start A_22157#query-call-clearAllChargeInformation
void PostgresBackend::clearAllChargeInformation(const db_model::HashedKvnr& kvnr)
{
    checkCommonPreconditions();
    mBackendChargeItem.clearAllChargeInformation(*transaction(), kvnr);
}
// GEMREQ-end A_22157#query-call-clearAllChargeInformation

// GEMREQ-start A_22157#query-call-clearAllChargeItemCommunications
void PostgresBackend::clearAllChargeItemCommunications(const db_model::HashedKvnr& kvnr)
{
    checkCommonPreconditions();
    TVLOG(2) << ::deleteChargeItemCommunicationsForKvnrStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "deletechargeitemcommunicationsforkvnrstatement");

    const auto result = transaction()->exec(deleteChargeItemCommunicationsForKvnrStatement.query,
                                            pqxx::params{kvnr.binarystring(),
                                                         static_cast<int>(model::Communication::messageTypeToInt(
                                                             model::Communication::MessageType::ChargChangeReq)),
                                                         static_cast<int>(model::Communication::messageTypeToInt(
                                                             model::Communication::MessageType::ChargChangeReply))});

    TVLOG(2) << "deleted " << result.size() << " results";
}
// GEMREQ-end A_22157#query-call-clearAllChargeItemCommunications

// GEMREQ-start A_22117-01#query-call-deleteCommunicationsForChargeItem
void PostgresBackend::deleteCommunicationsForChargeItem(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::deleteCommunicationsForChargeItemStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "deletecommunicationsforchargeitem");

    const auto result = transaction()->exec(
        deleteCommunicationsForChargeItemStatement.query,
        pqxx::params{
            taskId.toDatabaseId(), static_cast<int>(magic_enum::enum_integer(taskId.type())),
            static_cast<int>(model::Communication::messageTypeToInt(model::Communication::MessageType::ChargChangeReq)),
            static_cast<int>(
                model::Communication::messageTypeToInt(model::Communication::MessageType::ChargChangeReply))});

    TVLOG(2) << "deleted " << result.size() << " results";
}
// GEMREQ-end A_22117-01#query-call-deleteCommunicationsForChargeItem

uint64_t PostgresBackend::countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                                            const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    return mBackendChargeItem.countChargeInformationForInsurant(*transaction(), kvnr, search);
}

PostgresBackendTask& PostgresBackend::getTaskBackend(const model::PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            return mBackendTask;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            return mBackendTask162;
        case model::PrescriptionType::direkteZuweisung:
            return mBackendTask169;
        case model::PrescriptionType::direkteZuweisungPkv:
            return mBackendTask209;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            return mBackendTask200;
        case model::PrescriptionType::tRezept:
            return mBackendTask166;
    }
    Fail("invalid prescriptionType: " + std::to_string(std::uintmax_t(prescriptionType)));
}

void PostgresBackend::appendWherePrescriptionTypeIn(std::string& query,
                                                    const std::vector<model::PrescriptionType>& presciptionTypes)
{
    if (query.find("WHERE") == std::string::npos)
    {
        query.append(" WHERE prescription_type IN (");
    }
    else
    {
        query.append(" AND prescription_type IN (");
    }
    std::string prescriptionTypeParamStr;
    for (const auto& type : presciptionTypes)
    {
        if (! prescriptionTypeParamStr.empty())
        {
            prescriptionTypeParamStr.append(",");
        }
        prescriptionTypeParamStr.append(pqxx::to_string(gsl::narrow<int16_t>(magic_enum::enum_integer(type))))
            .append("::smallint");
    }
    query.append(prescriptionTypeParamStr).append(")");
}

PostgresBackend::~PostgresBackend (void) = default;

PostgresConnection& PostgresBackend::connection() const
{
    return mConnection;
}

PostgresConnection& PostgresBackend::mainConnection()
{
    static thread_local PostgresConnection connection{PostgresConnection::defaultConnectString()};
    return connection;
}

bool PostgresBackend::haveReadOnlyConnection()
{
    return Configuration::instance().getOptionalStringValue(ConfigurationKey::POSTGRES_RO_HOST).has_value();
}

PostgresConnection& PostgresBackend::readOnlyConnection()
{
    static thread_local PostgresConnection connection{PostgresConnection::readOnlyConnectString()};
    return connection;
}

uint64_t PostgresBackend::executeCountQuery(pqxx::transaction_base& transaction, const std::string_view& query,
                                            const db_model::Blob& paramValue, const std::optional<UrlArguments>& search,
                                            const std::string_view& context,
                                            const std::vector<model::PrescriptionType>& prescriptionTypeParam)
{
    std::string completeQuery(query);

    if (!prescriptionTypeParam.empty())
    {
        appendWherePrescriptionTypeIn(completeQuery, prescriptionTypeParam);
    }
    // Append an expression to the query for the search arguments, if there are any.
    if (search.has_value())
    {
        const auto whereExpression = search->getSqlWhereExpression(transaction.conn());
        if (! whereExpression.empty())
        {
            completeQuery += " AND ";
            completeQuery += whereExpression;
        }
    }

    TVLOG(1) << completeQuery;
    const pqxx::result result = transaction.exec(completeQuery, pqxx::params{paramValue.binarystring()});

    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of " + std::string(context) + " as int64_t");

    return gsl::narrow<uint64_t>(count);
}

void PostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << healthCheckQuery.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck");
    const auto result = transaction()->exec(healthCheckQuery.query);
    TVLOG(2) << "got " << result.size() << " results";
}

bool PostgresBackend::existsCountryCode(const std::string& countryCode)
{
    checkCommonPreconditions();
    TVLOG(2) << existsCountryCodeQuery.query;
    const auto durationTimer =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "existscountrycode");
    const auto result = transaction()->exec(existsCountryCodeQuery.query, pqxx::params{countryCode});
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expecting exactly one row as result.");
    return result.front().at(0).as<bool>();
}

void PostgresBackend::deleteEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    checkCommonPreconditions();
    TVLOG(2) << deleteEuAccessPermissionQuery.query;
    const auto durationTimer =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "deleteeuaccesspermission");
    const auto result = transaction()->exec(deleteEuAccessPermissionQuery.query, {kvnr.binarystring()}).no_rows();
    Expect(result.empty(), "Expected no rows to be returned by delete query.");
}

void PostgresBackend::createEuAccessPermission(const db_model::HashedKvnr& kvnr, const std::string& countryCode,
                                               const db_model::EncryptedBlob& accessCode, BlobId blobId,
                                               const db_model::Blob& salt, const model::Timestamp& validUntil)
{
    checkCommonPreconditions();
    TVLOG(2) << createEuAccessPermissionQuery.query;
    const auto durationTimer =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "createeuaccesspermission");
    const auto result =
        transaction()
            ->exec(createEuAccessPermissionQuery.query, {kvnr.binarystring(), countryCode, accessCode.binarystring(),
                                                         blobId, salt.binarystring(), validUntil.toXsDateTime()})
            .no_rows();
    Expect(result.empty(), "Expected no rows to be returned by create query.");
}

std::optional<db_model::EuAccessPermission>
PostgresBackend::retrieveEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveEuAccessPermissionQuery.query;
    const auto durationTimer =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "retrieveeuaccesspermission");
    const auto result = transaction()->exec(retrieveEuAccessPermissionQuery.query, {kvnr.binarystring()});
    Expect(result.size() <= 1, "Expected up to one row as result.");
    if (! result.empty())
    {

        constexpr pqxx::row::size_type countryCodeIdx = 0;
        constexpr pqxx::row::size_type accessCodeIdx = 1;
        constexpr pqxx::row::size_type blobIdIdx = 2;
        constexpr pqxx::row::size_type saltIdx = 3;
        constexpr pqxx::row::size_type expiresIdx = 4;

        const auto& r = result.front();
        Expect(r.size() == 5, "Expected exactly 5 columns as result.");
        return db_model::EuAccessPermission{r.at(countryCodeIdx).as<std::string>(),
                                            db_model::EncryptedBlob{r.at(accessCodeIdx).as<db_model::postgres_bytea>()},
                                            model::Timestamp{r.at(expiresIdx).as<double>()},
                                            r.at(blobIdIdx).as<BlobId>(),
                                            db_model::Blob{r.at(saltIdx).as<db_model::postgres_bytea>()}};
    }
    return std::nullopt;
}
