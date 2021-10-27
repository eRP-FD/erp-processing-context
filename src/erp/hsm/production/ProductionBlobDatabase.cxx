/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/production/ProductionBlobDatabase.hxx"

#include "erp/database/PostgresBackend.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "erp/erp-serverinfo.hxx"

#include <pqxx/except.hxx>
#include <pqxx/pqxx>
#include <unordered_set>
#include <vector>


namespace
{
    constexpr std::string_view akNameKey = "/akName";
    constexpr std::string_view pcrSetKey = "/pcrSet";


    BlobType getBlobType (const pqxx::row& row, const size_t columnIndex, const std::string& columnName)
    {
        Expect( ! row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null(), columnName + " is null");
        return static_cast<BlobType>(row[gsl::narrow<pqxx::row::size_type>(columnIndex)].as<int16_t>());
    }

    uint32_t getInteger (const pqxx::row& row, const size_t columnIndex, const std::string& columnName)
    {
        Expect( ! row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null(), columnName + " is null");
        return row[gsl::narrow<pqxx::row::size_type>(columnIndex)].as<int32_t>();
    }

    ErpVector getErpVector (const pqxx::row& row, const size_t columnIndex, const std::string& columnName)
    {
        Expect( ! row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null(), columnName + " is null");
        const auto data = row[gsl::narrow<pqxx::row::size_type>(columnIndex)].as<pqxx::binarystring>();
        ErpVector result (data.size());
        std::copy(data.begin(), data.end(), result.begin());
        return result;
    }

    SafeString getSafeString (const pqxx::row& row, const size_t columnIndex, const std::string& columnName)
    {
        Expect( ! row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null(), columnName + " is null");
        const auto& data = row[gsl::narrow<pqxx::row::size_type>(columnIndex)].as<pqxx::binarystring>();
        SafeString result (SafeString::no_zero_fill, data.size());
        std::copy(data.begin(), data.end(), static_cast<char*>(result));
        return result;
    }

    std::optional<std::chrono::system_clock::time_point> getOptionalTimePoint (const pqxx::row& row, const size_t columnIndex)
    {
        if (row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null())
            return std::nullopt;
        else
            return model::Timestamp(row.at(gsl::narrow<pqxx::row::size_type>(columnIndex)).as<double>()).toChronoTimePoint();
    }

    std::optional<std::string> getOptionalString (const pqxx::row& row, const size_t columnIndex)
    {
        if (row[gsl::narrow<pqxx::row::size_type>(columnIndex)].is_null())
            return std::nullopt;
        else
            return row.at(gsl::narrow<pqxx::row::size_type>(columnIndex)).as<std::string>();
    }

    std::optional<double> toOptionalSecondsSinceEpoch (const std::optional<std::chrono::system_clock::time_point>& timePoint)
    {
        if ( ! timePoint.has_value())
            return std::nullopt;
        else
            return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint->time_since_epoch()).count() / 1000.0;
    }

    std::string getHostIp (void)
    {
        return Configuration::instance().serverHost();
    }

    /**
     * Attestation keys (public keys or key pairs) have to be stored per host ip. Therefore
     * return the current host ip for the two attestation key types and an empty optional for all others.
     */
    std::optional<std::string> getHostIp (const BlobType type)
    {
        switch(type)
        {
            case BlobType::EndorsementKey:
            case BlobType::AttestationPublicKey:
            case BlobType::AttestationKeyPair:
                return Configuration::instance().serverHost();
            default:
                return std::nullopt;
        }
    }

    std::string getBuildNumber (void)
    {
        return std::string(ErpServerInfo::ReleaseVersion)
               + "/" + std::string(ErpServerInfo::BuildVersion)
               + "/" + std::string(ErpServerInfo::BuildType);
    }

    /**
     * Quotes are ties to specific builds or releases.
     */
    std::optional<std::string> getBuildNumber (const BlobType type)
    {
        if (type == BlobType::Quote)
            return getBuildNumber();
        else
            return std::nullopt;
    }

    std::string createMeta (
        const std::optional<ErpArray<TpmObjectNameLength>>& akName,
        const std::optional<PcrSet>& pcrSet)
    {
        std::ostringstream os;
        os << '{';
        if (akName.has_value())
        {
            os << '"' << akNameKey.substr(1) << '"'
               << ':'
               << '"' <<  Base64::encode(util::rawToBuffer(akName->data(), akName->size())) << '"';
        }
        if (akName.has_value() && pcrSet.has_value())
            os << ',';
        if (pcrSet.has_value())
        {
            os << '"' << pcrSetKey.substr(1) << '"'
               << ':'
               << pcrSet->toString();
        }
        os << '}';
        return os.str();
    }
}


ProductionBlobDatabase::ProductionBlobDatabase (const std::string& connectionString)
    : mConnection(connectionString)
{
}


ProductionBlobDatabase::ProductionBlobDatabase (void)
    : ProductionBlobDatabase(PostgresBackend::defaultConnectString())
{
}


BlobDatabase::Entry ProductionBlobDatabase::getBlob (
    BlobType type,
    BlobId id) const
{
    auto transaction = createTransaction();

    const pqxx::result result = transaction->exec_params(
        "SELECT blob_id, type, name, data, generation,"
        "       EXTRACT(EPOCH FROM expiry_date_time), EXTRACT(EPOCH FROM start_date_time), EXTRACT(EPOCH FROM end_date_time),"
        "       meta "
        "FROM erp.blob "
        "WHERE (type = $1 AND blob_id = $2)"
        "  AND (host_ip IS NULL OR host_ip = $3)"
        "  AND (build IS NULL OR build = $4)",
        gsl::narrow<int16_t>(type),
        gsl::narrow<int32_t>(id),
        transaction->esc(getHostIp()),
        transaction->esc(getBuildNumber()));

    Expect(result.size() > 0, "did not find the requested blob");
    Expect(result.size() == 1, "found more than one blob");

    Entry entry = convertEntry(result.front());

    transaction.commit();
    return entry;
}


std::vector<BlobDatabase::Entry> ProductionBlobDatabase::getAllBlobsSortedById (void) const
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction->exec_params(
        "SELECT blob_id, type, name, data, generation,"
        "       EXTRACT(EPOCH FROM expiry_date_time), EXTRACT(EPOCH FROM start_date_time), EXTRACT(EPOCH FROM end_date_time),"
        "       meta "
        "FROM erp.blob "
        "WHERE (host_ip IS NULL OR host_ip = $1)"
        "  AND (build IS NULL OR build = $2)"
        "ORDER BY blob_id",
        transaction->esc(getHostIp()),
        transaction->esc(getBuildNumber()));

    std::vector<Entry> entries;
    entries.reserve(result.size());
    TVLOG(1) << "got " << result.size() << " blobs from database";

    for (const auto& dbEntry : result)
        entries.emplace_back(convertEntry(dbEntry));

    transaction.commit();
    return entries;
}


BlobId ProductionBlobDatabase::storeBlob (Entry&& entry)
{
    const auto hostIp = getHostIp(entry.type);
    const auto build = getBuildNumber(entry.type);
    try
    {
        auto transaction = createTransaction();

        const pqxx::result result = transaction->exec_params(
            "INSERT INTO erp.blob (type, host_ip, build, name, data, generation, expiry_date_time, start_date_time, end_date_time, meta) "
            "VALUES ($1,$2,$3,$4,$5,$6, TO_TIMESTAMP($7), TO_TIMESTAMP($8), TO_TIMESTAMP($9), $10) "
            "RETURNING blob_id",
            static_cast<int16_t>(entry.type),
            hostIp,
            build,
            pqxx::binarystring(entry.name.data(), entry.name.size()),
            pqxx::binarystring(static_cast<const char*>(entry.blob.data), entry.blob.data.size()),
            gsl::narrow<int32_t>(entry.blob.generation),
            toOptionalSecondsSinceEpoch(entry.expiryDateTime),
            toOptionalSecondsSinceEpoch(entry.startDateTime),
            toOptionalSecondsSinceEpoch(entry.endDateTime),
            createMeta(entry.metaAkName, entry.metaPcrSet)
            );
        Expect(result.size() == 1, "insertion of new blob failed");

        transaction.commit();

        return getInteger(result.front(), 0, "blob_id");
    }
    catch (...)
    {
        processStoreBlobException();
    }
}


void ProductionBlobDatabase::deleteBlob (
    const BlobType type,
    const ErpVector& name)
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction->exec_params(
        "DELETE FROM erp.blob "
        "WHERE type = $1 AND name = $2",
        gsl::narrow<int16_t>(type),
        pqxx::binarystring(name.data(), name.size()));

    Expect(result.size() == 0, "did not expect an output");
    const auto count = result.affected_rows();
    ErpExpect(count==1, HttpStatus::NotFound, "did not find the blob");

    transaction.commit();
}


ProductionBlobDatabase::Transaction ProductionBlobDatabase::createTransaction (void) const
{
    mConnection.connectIfNeeded();
    return Transaction(mConnection);
}


std::vector<bool> ProductionBlobDatabase::hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const
{
    std::ostringstream query;
    query << "SELECT type, EXTRACT(EPOCH FROM expiry_date_time), EXTRACT(EPOCH FROM start_date_time), EXTRACT(EPOCH FROM end_date_time) FROM erp.blob "
          << "WHERE"
          << "      (host_ip IS NULL OR host_ip = $1)"
          << "  AND (build IS NULL OR build = $2)"
          << "  AND (";

    // We could convert blobTypes into a set to make sure that there are no duplicates but for the intended use
    // case this seems overkill and would make this method only needlessly complex.
    bool first = true;
    for (const auto& type : blobTypes)
    {
        if (first)
            first = false;
        else
            query << " OR";
        query << " type = " << static_cast<size_t>(type);
    }
    query << ")";

    auto transaction = createTransaction();
    const auto result = transaction->exec_params(
        query.str(),
        transaction->esc(getHostIp()),
        transaction->esc(getBuildNumber()));

    std::vector<bool> flags (blobTypes.size(), false);
    for (const auto& row : result)
    {
        Expect(row.size() == 4, "did not get a date/time for all requested columns");

        BlobDatabase::Entry entry;
        entry.type = getBlobType(row, 0, "type");
        entry.expiryDateTime = getOptionalTimePoint(row, 1);
        entry.startDateTime = getOptionalTimePoint(row, 2);
        entry.endDateTime = getOptionalTimePoint(row, 3);

        // Store the result.
        if (entry.isBlobValid())
        {
            for (size_t index=0; index<blobTypes.size(); ++index)
                if (blobTypes[index] == entry.type)
                    flags[index] = true;
        }
    }
    transaction.commit();

    return flags;
}


BlobDatabase::Entry ProductionBlobDatabase::convertEntry (const pqxx::row& dbEntry)
{
    Expect(dbEntry.size() == 9, "Got unexpected number of columns");
    Entry entry;
    entry.id = getInteger(dbEntry, 0, "blob_id");
    entry.type = getBlobType(dbEntry, 1, "type");
    entry.name = getErpVector(dbEntry, 2, "name");
    entry.blob.data = getSafeString(dbEntry, 3, "data");
    entry.blob.generation = getInteger(dbEntry, 4, "generation");
    entry.expiryDateTime = getOptionalTimePoint(dbEntry, 5);
    entry.startDateTime = getOptionalTimePoint(dbEntry, 6);
    entry.endDateTime = getOptionalTimePoint(dbEntry, 7);

    // The meta field contains a JSON structure.
    const auto meta = getOptionalString(dbEntry, 8);
    if (meta.has_value())
    {
        rapidjson::Document json;
        json.Parse(meta.value());
        if (json.HasParseError())
        {
            TLOG(WARNING) << "meta field in blob database has invalid JSON content";
            TVLOG(1) << "blob_id is " << entry.id << ", type is " << static_cast<size_t>(entry.type);

            // Ignore the error for now, i.e. leave the entry.meta* fields unset.
        }
        else
        {
            const auto akNameValue = rapidjson::Pointer(std::string(akNameKey)).Get(json);
            if (akNameValue != nullptr)
            {
                Expect(akNameValue->IsString(), "/akName value in meta field is not a string");
                entry.metaAkName = ErpArray<TpmObjectNameLength>::create(
                    Base64::decodeToString(
                        akNameValue->GetString()));
            }

            entry.metaPcrSet = PcrSet::fromJson(json, std::string(pcrSetKey));
        }
    }

    return entry;
}


ProductionBlobDatabase::Transaction::Transaction (pqxx::connection& connection)
    : mWork(connection)
{
}


pqxx::work* ProductionBlobDatabase::Transaction::operator-> (void)
{
    return &mWork;
}


void ProductionBlobDatabase::Transaction::commit (void)
{
    constexpr std::string_view error = "Error committing database transaction";
    try
    {
        mWork.commit();
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
    catch (const std::exception& e)
    {
        TLOG(WARNING) << error << " : " << e.what();
        ErpFail(HttpStatus::InternalServerError, std::string(error));
    }
    catch (...)
    {
        TLOG(WARNING) << error;
        ErpFail(HttpStatus::InternalServerError, std::string(error));
    }
}


void ProductionBlobDatabase::processStoreBlobException (void)
{
    try
    {
        const auto exception = std::current_exception();
        if (exception != nullptr)
            std::rethrow_exception(exception);
        else
            Fail("unknown error");
    }
    catch(pqxx::integrity_constraint_violation& e)
    {
        // Interpret all constraint violations as being caused by bad input data => BadRequest.
        TVLOG(1) << "integrity_constraint_violation: " << e.what();
        ErpFail(HttpStatus::BadRequest, "integrity_constraint_violation");
    }
    catch(...)
    {
        // Rethrow everything else unmodified.
        throw;
    }
}

