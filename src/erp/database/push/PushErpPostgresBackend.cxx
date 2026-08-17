/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushErpPostgresBackend.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/util/DurationCategory.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Hash.hxx"

#include <pqxx/binarystring>
#include <pqxx/result>

namespace
{
constexpr std::string_view selectBaseQuery{"SELECT pushkey_hashed, app_id_hashed, kvnr_hashed, blob_id, salt, payload, url, "
                                           "EXTRACT(EPOCH FROM last_modified) AS last_modified "
                                           "FROM erp.app_registrations"};
}

PushErpPostgresBackend::PushErpPostgresBackend(PostgresConnection& connection)
    : CommonPostgresBackend(connection)
    , mConnection(connection)
{
}

void PushErpPostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    constexpr std::string_view query = "SELECT FROM erp.app_registrations LIMIT 1";
    const auto dt =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck-erp.app_registrations");
    TVLOG(2) << query;
    const auto _ = transaction()->exec(query);
}

PostgresConnection& PushErpPostgresBackend::connection() const
{
    return mConnection;
}

std::optional<db_model::Pusher> PushErpPostgresBackend::findRegistration(const db_model::HashedId& hashedPushKey,
                                                                         const db_model::HashedId& hashedAppId,
                                                                         const db_model::HashedKvnr& kvnr) const
{
    checkCommonPreconditions();
    const std::string query{std::string{selectBaseQuery} + " WHERE pushkey_hashed = $1 AND app_id_hashed = $2 AND kvnr_hashed = $3"};
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "findregistration");
    TVLOG(2) << query;
    const auto result = transaction()->exec(query, pqxx::params{hashedPushKey.binarystring(), hashedAppId.binarystring(), kvnr.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        return createPusher(kvnr, result.front());
    }
    return std::nullopt;
}

std::vector<db_model::Pusher> PushErpPostgresBackend::getRegistrations(const db_model::HashedKvnr& kvnr) const
{
    checkCommonPreconditions();
    const std::string query{std::string{selectBaseQuery} + " WHERE kvnr_hashed = $1 ORDER BY last_modified ASC"};
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "getregistrations");
    TVLOG(2) << query;
    const auto result = transaction()->exec(query, pqxx::params{kvnr.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";
    std::vector<db_model::Pusher> ret;
    ret.reserve(gsl::narrow<size_t>(result.size()));
    for (const auto& row : result)
    {
        ret.emplace_back(createPusher(kvnr, row));
    }
    return ret;
}

// GEMREQ-start A_27157
void PushErpPostgresBackend::upsertRegistration(const db_model::Pusher& pusher,
                                                const db_model::EncryptionKey& encryptionKey)
{
    checkCommonPreconditions();
    A_27157.start("FdV-Instanz registrieren – Initiale Schlüsselableitung zur App-Registrierung speichern.");
    constexpr std::string_view upsert =
        "INSERT INTO erp.app_registrations (pushkey_hashed, app_id_hashed, kvnr_hashed, blob_id, "
        "salt, payload, url, encryption_key, time_created, "
        "last_modified) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
        "ON CONFLICT (pushkey_hashed, app_id_hashed, kvnr_hashed) DO UPDATE SET "
        "blob_id = EXCLUDED.blob_id, "
        "salt = EXCLUDED.salt, "
        "payload = EXCLUDED.payload, "
        "url = EXCLUDED.url, "
        "encryption_key = EXCLUDED.encryption_key, "
        "time_created = EXCLUDED.time_created, "
        "last_modified = EXCLUDED.last_modified";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "upsertregistration");
    TVLOG(2) << upsert;
    const auto _ =
        transaction()
            ->exec(upsert, pqxx::params{pqxx::binary_cast(pusher.hashedPushKey), pqxx::binary_cast(pusher.hashedAppId), pusher.kvnr.binarystring(),
                                        pusher.blobId, pusher.salt, pusher.payload, pusher.url,
                                        encryptionKey.encryptionKey,
                                        encryptionKey.timeCreated.toXsDateTime(), pusher.lastModified.toXsDateTime()})
            .no_rows();
}
// GEMREQ-end A_27157

void PushErpPostgresBackend::deleteRegistration(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                                const db_model::HashedId& hashedAppId)
{
    checkCommonPreconditions();
    constexpr std::string_view query =
        "DELETE FROM erp.app_registrations WHERE pushkey_hashed = $1 AND app_id_hashed = $2 AND kvnr_hashed = $3";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "deleteregistration");
    TVLOG(2) << query;
    const auto _ = transaction()->exec(query, pqxx::params{hashedPushKey.binarystring(), hashedAppId.binarystring(), kvnr.binarystring()}).no_rows();
}

void PushErpPostgresBackend::updateChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey,
                                            const db_model::HashedId& hashedAppId,
                                            const model::Channels& channels)
{
    checkCommonPreconditions();
    constexpr std::string_view query =
        "UPDATE erp.app_registrations SET subscribed_channel = $1::erp.push_notification_channel[], last_modified = $2 "
        "WHERE kvnr_hashed = $3 AND pushkey_hashed = $4 AND app_id_hashed = $5";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "updatechannels");
    TVLOG(2) << query;
    const auto _ =
        transaction()
                       ->exec(query, pqxx::params{channels.pqxxArrayStr(), model::Timestamp::now().toXsDateTime(),
                                                  kvnr.binarystring(), hashedPushKey.binarystring(), hashedAppId.binarystring()})
                       .no_rows();
}

std::pair<std::basic_string<std::byte>, std::optional<model::Channels>>
PushErpPostgresBackend::getChannels(const db_model::HashedKvnr& kvnr, const db_model::HashedId& hashedPushKey) const
{
    checkCommonPreconditions();
    constexpr std::string_view query =
        "SELECT subscribed_channel, app_id_hashed FROM erp.app_registrations WHERE kvnr_hashed = $1 AND pushkey_hashed = $2 order by last_modified desc limit 1";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "getchannelspushkey");
    TVLOG(2) << query;
    const auto result = transaction()->exec(query, pqxx::params{kvnr.binarystring(), hashedPushKey.binarystring()});
    Expect(result.size() <= 1, "Too many results in result set.");
    if (result.empty())
    {
        return {};
    }
    const auto row = result.front();
    return {row["app_id_hashed"].as<pqxx::bytes>(), channelsFromRow(row)};
}

bool PushErpPostgresBackend::isPushRegistered(const db_model::HashedKvnr& hashedKvnr, const model::ChannelId& channelId)
{
    checkCommonPreconditions();
    constexpr std::string_view query = "SELECT EXISTS (SELECT 1 FROM erp.app_registrations WHERE kvnr_hashed = $1 AND "
                                       "$2 = ANY(subscribed_channel))";
    const auto dt = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "ispushregistered");
    TVLOG(2) << query;
    const auto result = transaction()->exec(query, pqxx::params{hashedKvnr.binarystring(), to_string(channelId)});
    TVLOG(2) << "got " << result.size() << " results";
    return result.one_field().as<bool>();
}

db_model::Pusher PushErpPostgresBackend::createPusher(const db_model::HashedKvnr& kvnr, const pqxx::row& resultRow)
{
    std::optional<pqxx::bytes> hashedPushKey;
    std::optional<pqxx::bytes> hashedAppId;
    std::optional<BlobId> blobId;
    std::optional<db_model::Blob> salt;
    std::optional<db_model::EncryptedBlob> payload;
    std::optional<std::string> url;
    std::optional<std::string> keyIdentifier;
    std::optional<model::Timestamp> lastModified;

    for (const auto& field : resultRow)
    {
        auto enumValue = magic_enum::enum_cast<ColumnName>(field.name());
        Expect(enumValue.has_value(), "missing enum value for column " + std::string{field.name()});
        switch (*enumValue)
        {
            case ColumnName::pushkey_hashed:
                hashedPushKey.emplace(field.as<pqxx::bytes>());
                break;
            case ColumnName::app_id_hashed:
                hashedAppId.emplace(field.as<pqxx::bytes>());
                break;
            case ColumnName::kvnr_hashed:
                // already known from query
                break;
            case ColumnName::blob_id:
                blobId.emplace(field.as<BlobId>());
                break;
            case ColumnName::salt:
                salt.emplace(field.as<db_model::postgres_bytea>());
                break;
            case ColumnName::payload:
                payload.emplace(field.as<db_model::postgres_bytea>());
                break;
            case ColumnName::url:
                url.emplace(field.as<std::string>());
                break;
            case ColumnName::time_created:
            case ColumnName::encryption_key:
            case ColumnName::subscribed_channel:
                // not needed
                break;
            case ColumnName::last_modified:
                lastModified.emplace(model::Timestamp(field.as<double>()));
                break;
        }
    }
    Expect(hashedPushKey.has_value(), "pushKey not contained in query result");
    Expect(hashedAppId.has_value(), "appId not contained in query result");
    Expect(blobId.has_value(), "blobId not contained in query result");
    Expect(salt.has_value(), "salt not contained in query result");
    Expect(payload.has_value(), "payload not contained in query result");
    Expect(url.has_value(), "url not contained in query result");
    Expect(lastModified.has_value(), "lastModified not contained in query result");

    const db_model::HashedId hashedPushKey1(*hashedPushKey);
    const db_model::HashedId hashedAppId1(*hashedAppId);

    return db_model::Pusher{.hashedPushKey = hashedPushKey1,
                            .hashedAppId = hashedAppId1,
                            .kvnr = kvnr,
                            .blobId = *blobId,
                            .salt = *salt,
                            .payload = *payload,
                            .url = *url,
                            .lastModified = *lastModified};
}

model::Channels PushErpPostgresBackend::channelsFromRow(const pqxx::row& row)
{
    const auto sqlArray{row.front().as_sql_array<std::string>()};
    model::Channels channels;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = sqlArray.cbegin(); it != sqlArray.cend(); ++it)
    {
        channels.add(model::toChannelId(*it));
    }

    return channels;
}
