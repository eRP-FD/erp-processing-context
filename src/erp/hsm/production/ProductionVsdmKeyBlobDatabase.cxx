/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/model/Timestamp.hxx"

#include <pqxx/pqxx>

using postgres_bytea = std::basic_string<std::byte>;
using postgres_bytea_view = std::basic_string_view<std::byte>;

ProductionVsdmKeyBlobDatabase::ProductionVsdmKeyBlobDatabase(const std::string& connectionString)
    : mConnection(connectionString)
{
}


ProductionVsdmKeyBlobDatabase::ProductionVsdmKeyBlobDatabase()
    : ProductionVsdmKeyBlobDatabase(PostgresBackend::defaultConnectString())
{
}

ProductionVsdmKeyBlobDatabase::Entry ProductionVsdmKeyBlobDatabase::entryFromRow(const pqxx::row& rowEntry)
{
    Expect(rowEntry.size() == 5, "Got unexpected number of columns");
    ErpBlob blob;
    blob.data = SafeString{rowEntry[2].as<postgres_bytea>()};
    blob.generation = rowEntry[3].as<std::uint32_t>();
    const auto timestamp = model::Timestamp(rowEntry[4].as<double>()).toChronoTimePoint();

    return Entry{
        .operatorId = rowEntry[0].as<std::string>()[0],
        .version = rowEntry[1].as<std::string>()[0],
        .createdDateTime = timestamp,
        .blob = std::move(blob),
    };
}


ProductionVsdmKeyBlobDatabase::Entry ProductionVsdmKeyBlobDatabase::getBlob(char operatorId, char version) const
{
    std::lock_guard lock(mMutex);
    mConnection.connectIfNeeded();
    auto transaction = mConnection.createTransaction();

    const pqxx::result result =
        transaction->exec_params("SELECT operator, version, data, generation, EXTRACT(EPOCH FROM created_date_time) "
                                 "FROM erp.vsdm_key_blob "
                                 "WHERE (operator = $1 AND version = $2)",
                                 std::string{operatorId}, std::string{version});
    transaction->commit();
    ErpExpect(! result.empty(), HttpStatus::NotFound, "did not find the blob");
    Expect(result.size() == 1, "found more than one blob");
    return entryFromRow(result[0]);
}

std::vector<ProductionVsdmKeyBlobDatabase::Entry> ProductionVsdmKeyBlobDatabase::getAllBlobs() const
{
    std::lock_guard lock(mMutex);
    mConnection.connectIfNeeded();
    auto transaction = mConnection.createTransaction();
    const pqxx::result result =
        transaction->exec_params("SELECT operator, version, data, generation, EXTRACT(EPOCH FROM created_date_time) "
                                 "FROM erp.vsdm_key_blob");
    transaction->commit();
    std::vector<Entry> entries;
    entries.reserve(gsl::narrow<size_t>(result.size()));
    for (const auto& dbRow : result)
    {
        entries.emplace_back(entryFromRow(dbRow));
    }
    return entries;
}


void ProductionVsdmKeyBlobDatabase::storeBlob(Entry&& entry)
{
    std::lock_guard lock(mMutex);
    mConnection.connectIfNeeded();
    auto transaction = mConnection.createTransaction();
    try
    {
        std::chrono::duration<double> epochDouble = entry.createdDateTime.time_since_epoch();
        const pqxx::result result = transaction->exec_params(
            "INSERT INTO erp.vsdm_key_blob (operator, version, data, generation, created_date_time) "
            "VALUES ($1, $2, $3, $4, TO_TIMESTAMP($5))",
            std::string{entry.operatorId}, std::string{entry.version}, postgres_bytea_view(entry.blob.data),
            gsl::narrow<int32_t>(entry.blob.generation), epochDouble.count());
        transaction->commit();
    }
    catch (const pqxx::unique_violation& e)
    {
        TVLOG(1) << "unique_violation: " << e.what();
        ErpFail(HttpStatus::Conflict, "A VSDM Key Blob with that operator and version already exists");
    }
    catch (const pqxx::integrity_constraint_violation& e)
    {
        // Interpret all constraint violations as being caused by bad input data => BadRequest.
        TVLOG(1) << "integrity_constraint_violation: " << e.what();
        ErpFail(HttpStatus::BadRequest, "integrity_constraint_violation");
    }
}

void ProductionVsdmKeyBlobDatabase::deleteBlob(char operatorId, char version)
{
    std::lock_guard lock(mMutex);
    mConnection.connectIfNeeded();
    auto transaction = mConnection.createTransaction();
    try
    {
        const pqxx::result result = transaction->exec_params("DELETE FROM erp.vsdm_key_blob "
                                                             "WHERE (operator = $1 AND version = $2)",
                                                             std::string{operatorId}, std::string{version});
        transaction->commit();
        Expect(result.empty(), "did not expect an output");
        ErpExpect(result.affected_rows() == 1, HttpStatus::NotFound, "did not find the blob");
    }
    catch (const pqxx::in_doubt_error& /*inDoubtError*/)
    {
        // Exception that might be thrown in rare cases where the connection to the database is lost while finishing a
        // database transaction, and there's no way of telling whether it was actually executed by the backend. In this
        // case the database is left in an indeterminate (but consistent) state, and only manual inspection will tell
        // which is the case.
        constexpr std::string_view error_ = "Connection to database lost during committing a transaction. The "
                                            "transaction may or may not have been successful";
        TLOG(WARNING) << error_;
        ErpFail(HttpStatus::InternalServerError, std::string(error_));
    }
}

void ProductionVsdmKeyBlobDatabase::recreateConnection()
{
    std::lock_guard lock(mMutex);
    mConnection.recreateConnection();
}
