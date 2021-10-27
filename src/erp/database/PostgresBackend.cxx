/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/PostgresBackend.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/database/Data.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/model/Binary.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include <boost/algorithm/string.hpp>
#include <pqxx/pqxx>
#include <iostream>



struct PostgresBackend::QueryDefinition
{
    const char* name;
    const char* query;
};

namespace
{

#define QUERY(name, query) constexpr PostgresBackend::QueryDefinition name = {# name, query};
    QUERY(retrieveAllMedicationDispensesByKvnr, R"--(
        SELECT t.prescription_id, t.medication_dispense_bundle, t.medication_dispense_blob_id, a.salt
        FROM erp.task t
        LEFT JOIN erp.account a ON
            a.account_id = $1 AND a.master_key_type = 1 AND
            t.medication_dispense_blob_id = a.blob_id
        WHERE t.kvnr_hashed = $1
        AND ($2::bigint IS NULL OR prescription_id = $2::bigint)
        AND medication_dispense_bundle IS NOT NULL)--")
    QUERY(countAllMedicationDispensesByKvnr, R"--(
        SELECT COUNT(*)
        FROM erp.task t
        WHERE t.kvnr_hashed = $1
        AND medication_dispense_bundle IS NOT NULL)--")

    QUERY(retrieveTaskById                    , R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, secret
        FROM erp.task
        WHERE prescription_id = $1
        )--")
    QUERY(retrieveTaskByIdPlusReceipt         , R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            secret, receipt
        FROM erp.task
        WHERE prescription_id = $1
        )--")
    QUERY(retrieveTaskByIdPlusPrescription    , R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, healthcare_provider_prescription
        FROM erp.task
        WHERE prescription_id = $1
        FOR UPDATE
        )--")
    QUERY(retrieveAllTasksByKvnr              , R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id
        FROM erp.task
        WHERE kvnr_hashed = $1 AND status != 4
        )--")
    QUERY(countAllTasksByKvnr              , R"--(
        SELECT COUNT(*)
        FROM erp.task
        WHERE kvnr_hashed = $1 AND status != 4
        )--")

    QUERY(getTaskKeyData, R"--(
        SELECT task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on)
        FROM erp.task WHERE prescription_id = $1 FOR UPDATE)--")

    QUERY(retrieveCmac                        , "SELECT cmac FROM erp.vau_cmac WHERE valid_date = $1")
    // try to insert the new value
    // if that fails return the old value
    // ON CONFLICT DO UPDATE requires at least on assignment, therefore we set cmac to its current value.
    QUERY(acquireCmac                         , "INSERT INTO erp.vau_cmac (valid_date, cmac) VALUES ($1, $2) "
                                                "    ON CONFLICT(valid_date) DO UPDATE SET cmac=erp.vau_cmac.cmac "
                                                "    RETURNING cmac;")
    // the sent time is encoded in the ID.
    // It could be retrieved by erp.timestamp_from_suid(id) or erp.epoch_from_suuid(id)
    QUERY(insertCommunicationStatement,        "INSERT INTO erp.communication (id, message_type, sender, recipient, "
                                               "            received, prescription_id, sender_blob_id,"
                                               "            message_for_sender, recipient_blob_id, "
                                               "            message_for_recipient)"
                                               "     VALUES (erp.gen_suuid($1), $2, $3, $4, $5, $6, $7, $8, $9, $10)"
                                               "RETURNING id")
    QUERY(countRepresentativeCommunicationsStatement, "SELECT COUNT(*) FROM erp.communication "
                                                "    WHERE message_type = $1 AND prescription_id = $4 AND "
                                                "((sender = $2 AND recipient = $3) OR (sender = $3 AND recipient = $2))")
    QUERY(countCommunicationsByIdStatement, "SELECT COUNT(*) FROM erp.communication WHERE id = $1")
    QUERY(createTask                          , R"--(
        INSERT INTO erp.task (last_modified, authored_on, status) VALUES ($1, $2, $3)
            RETURNING prescription_id, EXTRACT(EPOCH FROM authored_on))--")
    QUERY(updateTask                          , R"--(
        UPDATE erp.task SET task_key_blob_id = $2, salt = $3, access_code = $4 WHERE prescription_id = $1
    )--")
    QUERY(updateTask_secret                   , R"--(
        UPDATE erp.task SET status = $2, last_modified = $3, secret = $4
        WHERE prescription_id = $1
        )--")
    QUERY(updateTask_activateTask, R"--(
        UPDATE erp.task
        SET kvnr = $2, kvnr_hashed = $3, last_modified = $4, expiry_date = $5, accept_date = $6, status = $7,
            healthcare_provider_prescription = $8
        WHERE prescription_id = $1
        )--")
    QUERY(updateTask_medicationDispenseReceipt,R"--(
        UPDATE erp.task
        SET status = $2, last_modified = $3, medication_dispense_bundle = $4, medication_dispense_blob_id =$5,
            receipt = $6, when_handed_over = $7, when_prepared = $8, performer = $9
        WHERE prescription_id = $1
        )--")
    QUERY(updateTask_deletePersonalData, R"--(
        UPDATE erp.task
        SET status = $2, last_modified = $3, kvnr = NULL, kvnr_hashed = NULL, salt = NULL, access_code = NULL,
            secret = NULL, healthcare_provider_prescription = NULL, receipt = NULL,
            when_handed_over = NULL, when_prepared = NULL, performer = NULL, medication_dispense_bundle = NULL
        WHERE prescription_id = $1
        )--")

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
    QUERY(countCommunicationsStatement, R"--(
        SELECT COUNT(*)
            FROM erp.communication
            WHERE
                (recipient = $1 OR sender = $1))--")

    QUERY(retrieveCommunicationIdsStatement,   "SELECT id FROM erp.communication WHERE recipient = $1")
    QUERY(deleteCommunicationStatement,        "DELETE FROM erp.communication WHERE id = $1::uuid AND sender = $2"
                                               "RETURNING id, EXTRACT(EPOCH FROM received)")
    QUERY(updateCommunicationsRetrievedStatement, R"--(
        UPDATE erp.communication
            SET received = $2
            WHERE
                ((received IS NULL)
                AND (id = ANY($1::uuid[]))
                AND (recipient = $3))
        )--")
    QUERY(deleteCommunicationsForTaskStament,  "DELETE FROM erp.communication WHERE prescription_id = $1")

    QUERY(insertAuditEventData,
          "INSERT INTO erp.auditevent (id, kvnr_hashed, event_id, action, agent_type, observer, prescription_id, metadata, blob_id) "
          "VALUES (erp.gen_suuid($1), $2, $3, $4, $5, $6, $7, $8, $9)"
          "RETURNING id")
    QUERY(retrieveAuditEventDataStatement, R"--(
          SELECT id, erp.epoch_from_suuid(id) AS recorded, event_id, action, agent_type, observer, prescription_id, metadata, blob_id
          FROM erp.auditevent
          WHERE
              kvnr_hashed = $1
              AND ($2::uuid IS NULL OR id = $2::uuid)
              AND ($3::bigint IS NULL OR prescription_id = $3::bigint))--")
    QUERY(countAuditEventDataStatement, R"--(
          SELECT COUNT(*)
          FROM erp.auditevent
          WHERE kvnr_hashed = $1)--")

    QUERY(insertOrReturnAccountSalt, R"--(
          SELECT erp.insert_or_return_account_salt($1::bytea, $2::smallint, $3::integer, $4::bytea))--")

    QUERY(retrieveSaltForAccount, R"--(
          SELECT salt FROM erp.account WHERE account_id = $1 AND master_key_type = $2 AND blob_id = $3)--")

    QUERY(healthCheckQuery, "SELECT FROM erp.task LIMIT 1")


    constexpr std::initializer_list<PostgresBackend::QueryDefinition> queries = {
        retrieveAllMedicationDispensesByKvnr,
        countAllMedicationDispensesByKvnr,
        createTask,
        updateTask,
        updateTask_secret,
        updateTask_medicationDispenseReceipt,
        updateTask_activateTask,
        updateTask_deletePersonalData,
        retrieveTaskById,
        retrieveTaskByIdPlusReceipt,
        retrieveTaskByIdPlusPrescription,
        retrieveAllTasksByKvnr,
        getTaskKeyData,
        retrieveCmac,
        acquireCmac,
        insertCommunicationStatement,
        countRepresentativeCommunicationsStatement,
        countCommunicationsByIdStatement,
        retrieveCommunicationsStatement,
        countCommunicationsStatement,
        retrieveCommunicationIdsStatement,
        deleteCommunicationStatement,
        updateCommunicationsRetrievedStatement,
        deleteCommunicationsForTaskStament,
        insertAuditEventData,
        countAuditEventDataStatement,
        retrieveAuditEventDataStatement,
        insertOrReturnAccountSalt,
        retrieveSaltForAccount,
        healthCheckQuery
    };
#undef QUERY


}  // anonymous namespace

struct PostgresBackend::TaskQueryIndexes
{
    pqxx::row::size_type prescriptionIdIndex = 0;
    pqxx::row::size_type kvnrIndex = 1;
    pqxx::row::size_type lastModifiedIndex = 2;
    pqxx::row::size_type authoredOnIndex = 3;
    pqxx::row::size_type expiryDateIndex = 4;
    pqxx::row::size_type acceptDateIndex = 5;
    pqxx::row::size_type statusIndex = 6;
    pqxx::row::size_type saltIndex = 7;
    pqxx::row::size_type keyBlobIdIndex = 8;
    std::optional<pqxx::row::size_type> accessCodeIndex = 9;
    std::optional<pqxx::row::size_type> secretIndex = 10;
    std::optional<pqxx::row::size_type> healthcareProviderPrescriptionIndex = {};
    std::optional<pqxx::row::size_type> receiptIndex = {};
};

std::string PostgresBackend::connectStringSslMode (void)
{
    const auto& configuration = Configuration::instance();

    const bool useSsl = configuration.getOptionalBoolValue(ConfigurationKey::POSTGRES_USESSL, true);
    const std::string serverRootCertPath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH,
        "/erp/config/POSTGRES_CERTIFICATE"); // default path not named POSTGRES_SSL_ROOT_CERTIFICATE for backward compatibility.
    const auto sslCertificatePath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH);
    const auto sslKeyPath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_KEY_PATH);

    if (serverRootCertPath.empty())
    {
        if (useSsl)
            return " sslmode=require";
        else
            return "";
    }
    else
    {
        std::string sslmode =  " sslmode=verify-full sslrootcert='" + serverRootCertPath + "'";
        if (sslCertificatePath.has_value() && !String::trim(sslCertificatePath.value()).empty()
            && sslKeyPath.has_value() && !String::trim(sslKeyPath.value()).empty())
        {
            const auto sslcert = String::trim(sslCertificatePath.value());
            const auto sslkey = String::trim(sslKeyPath.value());
            if ( ! (sslcert.empty() || sslkey.empty()))
                sslmode += " sslcert='" + sslcert + "' sslkey='" + sslkey + "'";
        }
        return sslmode;
    }
}


std::string PostgresBackend::defaultConnectString (void)
{
    const auto& configuration = Configuration::instance();

    const std::string host = configuration.getStringValue(ConfigurationKey::POSTGRES_HOST);
    const std::string port = configuration.getStringValue(ConfigurationKey::POSTGRES_PORT);
    const std::string user = configuration.getStringValue(ConfigurationKey::POSTGRES_USER);
    const std::string password = configuration.getStringValue(ConfigurationKey::POSTGRES_PASSWORD);
    const std::string dbname = configuration.getStringValue(ConfigurationKey::POSTGRES_DATABASE);
    const std::string connectTimeout = configuration.getStringValue(ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS);
    bool enableScramAuthentication = configuration.getOptionalBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION, false);
    const std::string tcpUserTimeoutMs = configuration.getStringValue(ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS);
    const std::string keepalivesIdleSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC);
    const std::string keepalivesIntervalSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC);
    const std::string keepalivesCountSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_COUNT);

    std::string sslmode = connectStringSslMode();

    std::string connectionString = "host='" + host
                                            + "' port='" + port
                                            + "' dbname='" + dbname
                                            + "' user='" + user + "'"
                                            + (password.empty() ? "" : " password='" + password + "'")
                                            + sslmode
                                            + " target_session_attrs=read-write"
                                            + " connect_timeout=" + connectTimeout
                                            + " tcp_user_timeout=" + tcpUserTimeoutMs
                                            + " keepalives=1"
                                            + " keepalives_idle=" + keepalivesIdleSec
                                            + " keepalives_interval=" + keepalivesIntervalSec
                                            + " keepalives_count=" + keepalivesCountSec
                                            + (enableScramAuthentication ? " channel_binding=require" : "");

    JsonLog(LogId::INFO)
        .message("postgres connection string")
        .keyValue("value", boost::replace_all_copy(connectionString, password, "******"));
    TVLOG(2) << "using connection string '" << boost::replace_all_copy(connectionString, password, "******") << "'";

    return connectionString;
}

#ifndef _WIN32
thread_local PostgresConnection PostgresBackend::mConnection{defaultConnectString()};
#else
// MSVC treats the thread_local static variable "mConnection" the same way as other static variables
// when it comes to initialization. As the order when static variables are initialized cannot be
// predicted "mConnection" is initialized before the static KeyMap of the Configuration instance.
// "defaultConnectionString" is passed to the constructor of "mConnection" accessing the Configuration
// instance which again tries to access the not yet existing KeyMap. This will result in a startup crash.
// As a workaround for MSVC and windows "mConnection" is defined as an instance variable. That has
// the consequence that on windows the connection is opened and closed each time a transaction is executed.
// This shouldn't be a problem as windows is not the target production system but only used during tests.
#endif


PostgresBackend::PostgresBackend (void)
    #ifdef _WIN32
    : mConnection{ defaultConnectString() }
    #endif
{
    mConnection.connectIfNeeded();
    TVLOG(1) << "transaction start";
    try
    {
        mTransaction = std::make_unique<pqxx::work>(mConnection);
    }
    catch (const pqxx::broken_connection& brokenConnection)
    {
        TVLOG(1) << "caught pqxx::broken_connection: " << brokenConnection.what();
        mConnection.close();
        mConnection.connectIfNeeded();
        TVLOG(1) << "transaction start 2nd try";
        mTransaction = std::make_unique<pqxx::work>(mConnection);
    }
    TVLOG(1) << "transaction started";
}

void PostgresBackend::commitTransaction()
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

bool PostgresBackend::isCommitted() const
{
    return !mTransaction;
}

void PostgresBackend::closeConnection()
{
    TVLOG(1) << "closing connection to database";
    mTransaction.reset();
    mConnection.close();
    TVLOG(2) << "connection closed";
}


void PostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << healthCheckQuery.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:healthCheck");

    const auto result = mTransaction->exec(healthCheckQuery.query);
    TVLOG(2) << "got " << result.size() << " results";
}


std::vector<db_model::MedicationDispense>
PostgresBackend::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnrHashed,
                                                const std::optional<model::PrescriptionId>& prescriptionId,
                                                const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveAllMedicationDispenses");

    std::string query = retrieveAllMedicationDispensesByKvnr.query;
    if (search.has_value())
    {
        query.append(search->getSqlExpression(mConnection, "                "));
    }
    TVLOG(2) << query;

    const pqxx::result results = mTransaction->exec_params(
        query, kvnrHashed.binarystring(),
        prescriptionId ? prescriptionId->toDatabaseId() : std::optional<std::int64_t>{});

    TVLOG(2) << "got " << results.size() << " results";

    if (prescriptionId.has_value())
    {
        ErpExpect(results.size() <= 1, HttpStatus::InternalServerError, "A maximum of one result row expected");
    }

    std::vector<db_model::MedicationDispense> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(results.size()));
    for (const auto& res : results)
    {
        Expect(res.size() == 4, "Too few fields in result row");
        Expect(not res.at(0).is_null(), "prescription_id is null.");
        Expect(not res.at(1).is_null(), "medication_dispense_bundle is null.");
        Expect(not res.at(2).is_null(), "medication_dispense_blob_id is null.");
        Expect(not res.at(3).is_null(), "salt is null.");
        auto id = model::PrescriptionId::fromDatabaseId(
                model::PrescriptionType::apothekenpflichigeArzneimittel,
                res[0].as<int64_t>());
        resultSet.emplace_back(std::move(id),
                               db_model::EncryptedBlob{res[1].as<pqxx::binarystring>()},
                               gsl::narrow<BlobId>(res[2].as<int32_t>()),
                               db_model::Blob{res[3].as<pqxx::binarystring>()});
    }
    return resultSet;
}

uint64_t PostgresBackend::countAllMedicationDispenses(
    const db_model::HashedKvnr& kvnr,
    const std::optional<UrlArguments>& search)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countAllMedicationDispenses");
    return executeCountQuery(countAllMedicationDispensesByKvnr.query, kvnr, search, "medication dispenses");
}

CmacKey PostgresBackend::acquireCmac(const date::sys_days& validDate, RandomSource& randomSource)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:acquireCmac");

    std::ostringstream validDateStrm;
    validDateStrm << date::year_month_day{validDate};
    { // scope
        TVLOG(2) << ::retrieveCmac.query;
        auto selectResult = mTransaction->exec_params(retrieveCmac.query, validDateStrm.str());
        TVLOG(2) << "got " << selectResult.size() << " results";
        if (!selectResult.empty())
        {
            Expect(selectResult.size() == 1, "Expected only one CMAC.");
            auto cmac = selectResult.front().at(0).as<pqxx::binarystring>();
            return CmacKey::fromBin(cmac.view());
        }
    }
    const auto& newKey = CmacKey::randomKey(randomSource);
    const pqxx::binarystring newKeyBin(newKey.data(), newKey.size());
    { // scope
        TVLOG(2) << ::acquireCmac.query;
        auto acquireResult = mTransaction->exec_params(::acquireCmac.query, validDateStrm.str(), newKeyBin);
        TVLOG(2) << "got " << acquireResult.size() << " results";
        Expect(acquireResult.size() == 1, "Expected exactly one CMAC.");
        auto cmac = acquireResult.front().at(0).as<pqxx::binarystring>();
        return CmacKey::fromBin(cmac.view());
    }
}

std::tuple<model::PrescriptionId, model::Timestamp> PostgresBackend::createTask(model::Task::Status taskStatus,
                                                                                const model::Timestamp& lastUpdated,
                                                                                const model::Timestamp& created)
{
    checkCommonPreconditions();
    TVLOG(2) << ::createTask.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:createTask");

    const auto status = gsl::narrow<int>(model::Task::toNumericalStatus(taskStatus));
    auto result = mTransaction->exec_params1(::createTask.query,
                                             lastUpdated.toXsDateTime(),
                                             created.toXsDateTime(),
                                             status);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 2, "Expected Prescription Id and athored on as returned values.");
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                                                result[0].as<int64_t>());
    model::Timestamp authoredOn{result[1].as<double>()};
    return std::make_tuple(std::move(prescriptionId), std::move(authoredOn));
}

void PostgresBackend::updateTask(const model::PrescriptionId& taskId,
                                  const db_model::EncryptedBlob& accessCode,
                                  uint32_t blobId,
                                  const db_model::Blob& salt)
{
    checkCommonPreconditions();
    TVLOG(2) << ::updateTask.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTask");

    auto result = mTransaction->exec_params(::updateTask.query,
                                              taskId.toDatabaseId(),
                                              blobId,
                                              salt.binarystring(),
                                              accessCode.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "No result rows expected");
}

std::tuple<BlobId, db_model::Blob, model::Timestamp>
PostgresBackend::getTaskKeyData(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::getTaskKeyData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:getTaskKeyData");

    auto result = mTransaction->exec_params(::getTaskKeyData.query, taskId.toDatabaseId());
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expected exactly one set of key data");
    const auto blobId = result.front().at(0).as<int64_t>();
    const db_model::Blob salt{result.front().at(1).as<pqxx::binarystring>()};

    const model::Timestamp authoredOn{result.front().at(2).as<double>()};
    return std::make_tuple(gsl::narrow_cast<BlobId>(blobId), std::move(salt), authoredOn);
}

void PostgresBackend::updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                                 model::Task::Status status,
                                                 const model::Timestamp& lastModifiedDate,
                                                 const std::optional<db_model::EncryptedBlob>& secret)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskStatusAndSecret");

    // It is possible to set or delete the secret using this query
    std::optional<pqxx::binarystring> secretBin;
    if (secret.has_value())
    {
        secretBin = secret->binarystring();
    }

    TVLOG(2) << updateTask_secret.query;
    const pqxx::result result =
        mTransaction->exec_params(updateTask_secret.query, taskId.toDatabaseId(),
                                    static_cast<int>(model::Task::toNumericalStatus(status)),
                                    lastModifiedDate.toXsDateTime(), secretBin);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}

void PostgresBackend::activateTask (
    const model::PrescriptionId& taskId,
    const db_model::EncryptedBlob& encryptedKvnr,
    const db_model::HashedKvnr& hashedKvnr,
    model::Task::Status taskStatus,
    const model::Timestamp& lastModified,
    const model::Timestamp& expiryDate,
    const model::Timestamp& acceptDate,
    const db_model::EncryptedBlob& healthCareProviderPrescription)
{
    checkCommonPreconditions();
    TVLOG(2) << updateTask_activateTask.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:activateTask");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result = mTransaction->exec_params(
        updateTask_activateTask.query,
        taskId.toDatabaseId(), encryptedKvnr.binarystring(), hashedKvnr.binarystring(), lastModified.toXsDateTime(),
        expiryDate.toXsDateTime(), acceptDate.toXsDateTime(), static_cast<int>(status),
        healthCareProviderPrescription.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}

void PostgresBackend::updateTaskMedicationDispenseReceipt(
    const model::PrescriptionId& taskId,
    const model::Task::Status& taskStatus,
    const model::Timestamp& lastModified,
    const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId,
    const db_model::HashedTelematikId& telematicId,
    const model::Timestamp& whenHandedOver,
    const std::optional<model::Timestamp>& whenPrepared,
    const db_model::EncryptedBlob& receipt)
{
    checkCommonPreconditions();
    TVLOG(2) << updateTask_medicationDispenseReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskMedicationDispenseReceipt");

    const auto status = model::Task::toNumericalStatus(taskStatus);
    const pqxx::result result = mTransaction->exec_params(
        updateTask_medicationDispenseReceipt.query,
        taskId.toDatabaseId(),
        static_cast<int>(status), lastModified.toXsDateTime(),
        medicationDispense.binarystring(),
        gsl::narrow<int32_t>(medicationDispenseBlobId),
        receipt.binarystring(),
        whenHandedOver.toXsDateTime(),
        whenPrepared ? std::make_optional(whenPrepared->toXsDateTime()) : std::nullopt,
        telematicId.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}

void PostgresBackend::updateTaskClearPersonalData(const model::PrescriptionId& taskId,
                                                   model::Task::Status taskStatus,
                                                   const model::Timestamp& lastModified)
{
    checkCommonPreconditions();
    TVLOG(2) << updateTask_deletePersonalData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskClearPersnalData");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result = mTransaction->exec_params(
        updateTask_deletePersonalData.query,
        taskId.toDatabaseId(),
        static_cast<int>(status),
        lastModified.toXsDateTime());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}

std::string PostgresBackend::storeAuditEventData(db_model::AuditData& auditData)
{
    checkCommonPreconditions();
    TVLOG(2) << insertAuditEventData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:storeAuditEventData");

    const std::string actionString(1, static_cast<char>(auditData.action));
    const auto recorded = model::Timestamp::now();

    const pqxx::result result = mTransaction->exec_params(
        insertAuditEventData.query,
        recorded.toXsDateTime(),
        auditData.insurantKvnr.binarystring(),
        static_cast<std::int16_t>(auditData.eventId),
        actionString,
        static_cast<std::int16_t>(auditData.agentType),
        auditData.deviceId,
        auditData.prescriptionId.has_value() ? auditData.prescriptionId->toDatabaseId() : std::optional<int64_t>(),
        auditData.metaData.has_value() ?  auditData.metaData->binarystring() : std::optional<pqxx::binarystring>(),
        auditData.blobId);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1 && result.front().size() == 1, "Expected one result");
    auto sid = result.front().at(0).as<std::string>();
    Uuid id(sid);
    auditData.id = id;
    auditData.recorded = recorded;

    return id;
}

std::vector<db_model::AuditData> PostgresBackend::retrieveAuditEventData(
        const db_model::HashedKvnr& kvnr,
        const std::optional<Uuid>& id,
        const std::optional<model::PrescriptionId>& prescriptionId,
        const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveAuditEventData");

    std::string query = retrieveAuditEventDataStatement.query;

    // Append an expression to the query for the search, sort and paging arguments, if there are any.
    A_19398.start("Add SQL expressions for searching, sorting and paging");
    if (search.has_value())
        query += search->getSqlExpression(mConnection, "                ");
    A_19398.finish();

    std::optional<std::string> idStr = id.has_value() ? id->toString() : std::optional<std::string>{};
    std::optional<int64_t> prescriptionIdNum =
        prescriptionId.has_value() ? prescriptionId->toDatabaseId() : std::optional<int64_t>{};

    // Run the query. Arguments that are passed on to exec_params are quoted and escaped automatically.
    A_19396.start("Filter audit events by Kvnr as part of the query");
    TVLOG(2) << query;
    const pqxx::result results = mTransaction->exec_params(
        query,
        kvnr.binarystring(),
        idStr,
        prescriptionIdNum);
    A_19396.finish();

    TVLOG(2) << "got " << results.size() << " results";

    std::vector<db_model::AuditData> resultSet;
    for (const auto& res : results)
    {
        Expect(res.size() == 9, "Query result has unexpected number of values");
        const auto resultId = res.at(0).as<std::string>();
        const auto eventId = res.at(2).as<std::int16_t>();
        Expect(eventId >= 0 && eventId <= static_cast<std::int16_t>(model::AuditEventId::MAX), "Invalid AuditEvent id");
        const auto actionStr = res.at(3).as<std::string>();
        const auto agentType = magic_enum::enum_cast<model::AuditEvent::AgentType>(res.at(4).as<std::int16_t>());
        Expect(agentType, "Invalid AgentType");
        Expect(actionStr.size() == 1, "AuditEvent action must be single character");
        const auto deviceId = res.at(5).as<std::int16_t>();
        const auto prescriptionIdRes = res.at(6).is_null() ? std::optional<model::PrescriptionId>() :
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                                  res.at(6).as<std::int64_t>());
        const auto metaData = res.at(7).is_null() ? std::optional<db_model::EncryptedBlob>()
                                                  : db_model::EncryptedBlob(res.at(7).as<pqxx::binarystring>());
        const auto blobId = res.at(8).is_null() ? std::optional<BlobId>() : res.at(8).as<int>();

        db_model::AuditData auditData(
            *agentType, static_cast<model::AuditEventId>(eventId), metaData,
            model::AuditEvent::ActionNamesReverse.at(actionStr), kvnr, deviceId, prescriptionIdRes, blobId);
        auditData.id = resultId;
        auditData.recorded = model::Timestamp(res.at(1).as<double>());
        resultSet.push_back(std::move(auditData));
    }

    return resultSet;
}

uint64_t PostgresBackend::countAuditEventData(
    const db_model::HashedKvnr& kvnr,
    const std::optional<UrlArguments>& search)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countAuditEventData");
    return executeCountQuery(countAuditEventDataStatement.query, kvnr, search, "audit events");
}


std::optional<db_model::Task> PostgresBackend::retrieveTaskBasics(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveTaskById.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskBasics");

    const pqxx::result result = mTransaction->exec_params(retrieveTaskById.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        return taskFromQueryResultRow(result.front(), TaskQueryIndexes());
    }
    return {};
}

std::optional<db_model::Task> PostgresBackend::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskForUpdate");

    std::string query(retrieveTaskById.query);
    query.append("    FOR UPDATE");
    TVLOG(2) << query;
    const pqxx::result result = mTransaction->exec_params(query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        return taskFromQueryResultRow(result.front(), TaskQueryIndexes());
    }
    return {};
}


std::optional<db_model::Task> PostgresBackend::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveTaskByIdPlusReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskAndReceipt");

    const pqxx::result result = mTransaction->exec_params(retrieveTaskByIdPlusReceipt.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        Expect(result.front().size() == 11,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        TaskQueryIndexes indexes;
        indexes.accessCodeIndex.reset();
        indexes.secretIndex = 9;
        indexes.receiptIndex = 10;
        return taskFromQueryResultRow(result.front(), indexes);
    }
    return {};
}

std::optional<db_model::Task>
PostgresBackend::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveTaskByIdPlusPrescription.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskAndPrescription");

    const pqxx::result result = mTransaction->exec_params(retrieveTaskByIdPlusPrescription.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (!result.empty())
    {
        Expect(result.front().size() == 11,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        TaskQueryIndexes indexes;
        indexes.secretIndex.reset();
        indexes.healthcareProviderPrescriptionIndex = 10;
        return taskFromQueryResultRow(result.front(), indexes);
    }
    return {};
}

std::vector<db_model::Task> PostgresBackend::retrieveAllTasksForPatient (
    const db_model::HashedKvnr& kvnrHashed,
    const std::optional<UrlArguments>& search)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveAllTasksForPatient");

    std::string query = retrieveAllTasksByKvnr.query;
    if (search.has_value())
    {
        A_19569_02.start("Add search parameter to query");
        query.append(search->getSqlExpression(mConnection, "                "));
        A_19569_02.finish();
    }
    TVLOG(2) << query;

    const pqxx::result results = mTransaction->exec_params(query, kvnrHashed.binarystring());

    TaskQueryIndexes indexes;
    indexes.accessCodeIndex.reset();
    indexes.secretIndex.reset();

    TVLOG(2) << "got " << results.size() << " results";
    std::vector<db_model::Task> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(results.size()));

    for (const auto& res : results)
    {
        Expect(res.size() == 9, "Invalid number of fields in result row: " + std::to_string(res.size()));
        resultSet.emplace_back(taskFromQueryResultRow(res, indexes));
    }
    return resultSet;
}

uint64_t PostgresBackend::countAllTasksForPatient(
    const db_model::HashedKvnr& kvnr,
    const std::optional<UrlArguments>& search)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countAllTasksForPatient");
    return executeCountQuery(countAllTasksByKvnr.query, kvnr, search, "tasks");
}

std::optional<Uuid> PostgresBackend::insertCommunication(const model::PrescriptionId& prescriptionId,
                                                         const model::Timestamp& timeSent,
                                                         const model::Communication::MessageType messageType,
                                                         const db_model::HashedId& sender,
                                                         const db_model::HashedId& recipient,
                                                         const std::optional<model::Timestamp>& timeReceived,
                                                         BlobId senderBlobId,
                                                         const db_model::EncryptedBlob& messageForSender,
                                                         BlobId recipientBlobId,
                                                         const db_model::EncryptedBlob& messageForRecipient)
{
    checkCommonPreconditions();
    TVLOG(2) << insertCommunicationStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:insertCommunication");

    const pqxx::result result = mTransaction->exec_params(
        insertCommunicationStatement.query,
                    timeSent.toXsDateTime(),
                    static_cast<int>(model::Communication::messageTypeToInt(messageType)),
                    sender.binarystring(),
                    recipient.binarystring(),
                    timeReceived ? std::make_optional(timeReceived->toXsDateTime()) : std::nullopt,
                    prescriptionId.toDatabaseId(),
                    gsl::narrow<int32_t>(senderBlobId),
                    messageForSender.binarystring(),
                    gsl::narrow<int32_t>(recipientBlobId),
                    messageForRecipient.binarystring());
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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countRepresentativeCommunications");

    const auto result = mTransaction->exec_params(
        countRepresentativeCommunicationsStatement.query,
        /* $1 */ static_cast<int>(model::Communication::messageTypeToInt(model::Communication::MessageType::Representative)),
        /* $2 */ insurantA.binarystring(),
        /* $3 */ insurantB.binarystring(),
        /* $4 */ prescriptionId.toDatabaseId());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of communications as int64");

    return gsl::narrow<uint64_t>(count);
}

bool PostgresBackend::existCommunication(const Uuid& communicationId)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:existCommunication");

    const pqxx::result result =
        mTransaction->exec_params(countCommunicationsByIdStatement.query, communicationId.toString());

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveCommunications");

    // We don't use a prepared statement for this query because search arguments lead to dynamically generated
    // WHERE and SORT clauses. Paging can add LIMIT and OFFSET expressions.
    A_19520.start("filtering by user as either sender or recipient is done by the constant part of the query");
    std::string query = retrieveCommunicationsStatement.query;
    A_19520.finish();

    // Append a an expression to the query for the search, sort and paging arguments, if there are any.
    A_19534.start("add SQL expressions for searching, sorting and paging");
    if (search.has_value())
        query += search->getSqlExpression(mConnection, "                ");
    A_19534.finish();

    std::optional<std::string> communicationIdStr =
        communicationId ? communicationId->toString() : std::optional<std::string>{};

    // Run the query. Arguments that are passed on to exec_params are quoted and escaped automatically.
    TVLOG(2) << query;
    const pqxx::result results = mTransaction->exec_params(
            query,
            user.binarystring(),
            communicationIdStr);

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
        newComm.communication = db_model::EncryptedBlob{item[2].as<pqxx::binarystring>()};
        newComm.blobId = gsl::narrow<BlobId>(item[3].as<int>());
        newComm.salt = db_model::Blob{item[4].as<pqxx::binarystring>()};
    }

    return communications;
}


uint64_t PostgresBackend::countCommunications(
    const db_model::HashedId& user,
    const std::optional<UrlArguments>& search)
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countCommunications");
    return executeCountQuery(countCommunicationsStatement.query, user, search, "communications");
}


std::vector<Uuid> PostgresBackend::retrieveCommunicationIds (const db_model::HashedId& recipient)
{
    checkCommonPreconditions();
    TVLOG(2) << retrieveCommunicationIdsStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveCommunicationIds");

    const pqxx::result results = mTransaction->exec_params(
        retrieveCommunicationIdsStatement.query,
        recipient.binarystring());

    TVLOG(2) << "got " << results.size() << " results";

    std::vector<Uuid> ids;
    ids.reserve(gsl::narrow<size_t>(results.size()));
    for (const auto& item : results)
        ids.emplace_back(Uuid(item.at(0).as<std::string>()));

    return ids;
}


std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>> PostgresBackend::deleteCommunication(
    const Uuid& communicationId,
    const db_model::HashedId& sender)
{
    checkCommonPreconditions();
    TVLOG(2) << deleteCommunicationStatement.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:deleteCommunication");

    const pqxx::result result = mTransaction->exec_params(
        deleteCommunicationStatement.query,
        communicationId.toString(), sender.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() <= 1, "Expecting either no or one row as result.");
    if (result.size() == 1)
    {
        Expect(result.front().size() == 2, "Expecting two columns as result containing id and time received.");
        const auto id = Uuid(result.front().at(0).as<std::string>());
        const auto secondsSinceEpoch = result.front().at(1);
        if (secondsSinceEpoch.is_null())
            return {id, {}};
        else
            return {id, model::Timestamp(secondsSinceEpoch.as<double>())};
    }
    else
        return {{},{}};
}


void PostgresBackend::markCommunicationsAsRetrieved (
    const std::vector<Uuid>& communicationIds,
    const model::Timestamp& retrieved,
    const db_model::HashedId& recipient)
{
    checkCommonPreconditions();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:markCommunicationsAsRetrieved");

    std::vector<std::string> communicationIdStrings;
    communicationIdStrings.reserve(communicationIds.size());
    for (const auto& uuid : communicationIds)
    {
        communicationIdStrings.emplace_back(uuid.toString());
    }

    TVLOG(2) << updateCommunicationsRetrievedStatement.query;
    const auto result = mTransaction->exec_params(
        updateCommunicationsRetrievedStatement.query,
        communicationIdStrings,
        retrieved.toXsDateTime(),
        recipient.binarystring());
    TVLOG(2) << "got " << result.size() << " results";
}

void PostgresBackend::deleteCommunicationsForTask (const model::PrescriptionId& taskId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::deleteCommunicationsForTaskStament.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:deleteCommunicationsForTask");

    const auto result = mTransaction->exec_params(deleteCommunicationsForTaskStament.query, taskId.toDatabaseId());
    TVLOG(2) << "got " << result.size() << " results";
}

std::optional<db_model::Blob>
PostgresBackend::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                           db_model::MasterKeyType masterKeyType,
                                           BlobId blobId,
                                           const db_model::Blob& salt)
{
    checkCommonPreconditions();
    TVLOG(2) << ::insertOrReturnAccountSalt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:insertOrReturnAccountSalt");

    auto result = mTransaction->exec_params(::insertOrReturnAccountSalt.query,
                                              accountId.binarystring(),
                                              gsl::narrow<int>(masterKeyType),
                                              gsl::narrow<int32_t>(blobId),
                                              salt.binarystring());
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expected exactly one row.");
    Expect(result.front().size() == 1, "Expected exactly one column.");
    if (result[0][0].is_null())
    {
        return std::nullopt;
    }
    return std::make_optional<db_model::Blob>(result[0][0].as<pqxx::binarystring>());
}

std::optional<db_model::Blob> PostgresBackend::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                      db_model::MasterKeyType masterKeyType,
                                                                      BlobId blobId)
{
    checkCommonPreconditions();
    TVLOG(2) << ::retrieveSaltForAccount.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveSaltForAccount");

    auto result = mTransaction->exec_params(::retrieveSaltForAccount.query,
                                              accountId.binarystring(),
                                              gsl::narrow<int>(masterKeyType),
                                              gsl::narrow<int32_t>(blobId));
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Expected at most one salt");
    if (result.empty())
    {
        return std::nullopt;
    }
    return db_model::Blob(result[0].at(0).as<pqxx::binarystring>());
}

db_model::Task PostgresBackend::taskFromQueryResultRow(const pqxx::row& resultRow, const TaskQueryIndexes& indexes)
{
    Expect(resultRow.size() >= 8, "Too few fields in result row");

    const auto& prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                              resultRow.at(indexes.prescriptionIdIndex).as<int64_t>());
    const auto keyBlobId =
        gsl::narrow<BlobId>(resultRow.at(indexes.keyBlobIdIndex).as<int32_t>());
    const auto status =
            model::Task::fromNumericalStatus(gsl::narrow<int8_t>(resultRow.at(indexes.statusIndex).as<int>()));

    db_model::Blob salt;
    if (resultRow.at(indexes.saltIndex).is_null())
    {
        Expect3(status == model::Task::Status::cancelled, "Missing salt data in task.", std::logic_error);
    }
    else
    {
        salt = db_model::Blob{resultRow.at(indexes.saltIndex).as<pqxx::binarystring>()};
    }

    const model::Timestamp authoredOn{resultRow.at(indexes.authoredOnIndex).as<double>()};
    const model::Timestamp lastModified{resultRow.at(indexes.lastModifiedIndex).as<double>()};
    db_model::Task dbTask{prescriptionId, keyBlobId, salt, status, authoredOn, lastModified};

    if (indexes.accessCodeIndex.has_value() && !resultRow.at(*indexes.accessCodeIndex).is_null())
    {
        dbTask.accessCode.emplace(resultRow.at(*indexes.accessCodeIndex).as<pqxx::binarystring>());
    }

    if (!resultRow.at(indexes.kvnrIndex).is_null())
    {
        dbTask.kvnr.emplace(resultRow.at(indexes.kvnrIndex).as<pqxx::binarystring>());
    }

    if (!resultRow.at(indexes.expiryDateIndex).is_null())
    {
        dbTask.expiryDate.emplace(resultRow.at(indexes.expiryDateIndex).as<double>());
    }

    if (!resultRow.at(indexes.acceptDateIndex).is_null())
    {
        dbTask.acceptDate.emplace(resultRow.at(indexes.acceptDateIndex).as<double>());
    }

    if (indexes.secretIndex.has_value() && !resultRow.at(*indexes.secretIndex).is_null())
    {
        dbTask.secret.emplace(resultRow.at(*indexes.secretIndex).as<pqxx::binarystring>());
    }

    if (indexes.receiptIndex.has_value() && !resultRow.at(*indexes.receiptIndex).is_null())
    {
        dbTask.receipt.emplace(resultRow.at(*indexes.receiptIndex).as<pqxx::binarystring>());
    }

    if (indexes.healthcareProviderPrescriptionIndex.has_value() &&
        !resultRow.at(*indexes.healthcareProviderPrescriptionIndex).is_null())
    {
        dbTask.healthcareProviderPrescription.emplace(
            resultRow.at(*indexes.healthcareProviderPrescriptionIndex).as<pqxx::binarystring>());
    }

    return dbTask;
}


void PostgresBackend::checkCommonPreconditions()
{
    Expect3(mTransaction, "Transaction already committed", std::logic_error);
}


uint64_t PostgresBackend::executeCountQuery(
    const std::string_view& query,
    const db_model::Blob& paramValue,
    const std::optional<UrlArguments>& search,
    const std::string_view& context)
{
    checkCommonPreconditions();

    std::string completeQuery(query);

    // Append an expression to the query for the search arguments, if there are any.
    if(search.has_value())
    {
        const auto whereExpression = search->getSqlWhereExpression(mConnection);
        if(!whereExpression.empty())
        {
            completeQuery += " AND ";
            completeQuery += whereExpression;
        }
    }

    TVLOG(2) << completeQuery;
    const pqxx::result result = mTransaction->exec_params(
            completeQuery,
            paramValue.binarystring());

    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of" + std::string(context) + " as int64");

    return gsl::narrow<uint64_t>(count);
}


PostgresBackend::~PostgresBackend (void) = default;
