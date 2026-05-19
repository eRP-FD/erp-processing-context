/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/PostgresConnection.hxx"

#include "shared/database/PostgresConnectString.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <boost/algorithm/string.hpp>
#include <pqxx/nontransaction>
#include <pqxx/transaction>

PostgresConnection::PostgresConnection(const std::string_view& connectionString)
    : mConnectionString(connectionString)
    , mConnection()
{
}

std::string PostgresConnection::defaultConnectString()
{
    const auto& configuration = Configuration::instance();
    return PostgresConnectString{
        .host = configuration.getStringValue(ConfigurationKey::POSTGRES_HOST),
        .port = configuration.getStringValue(ConfigurationKey::POSTGRES_PORT),
        .user = configuration.getStringValue(ConfigurationKey::POSTGRES_USER),
        .password = configuration.getStringValue(ConfigurationKey::POSTGRES_PASSWORD),
        .dbname = configuration.getStringValue(ConfigurationKey::POSTGRES_DATABASE),
        .connectTimeout = configuration.getStringValue(ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS),
        .enableScramAuthentication = configuration.getBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION),
        .tcpUserTimeoutMs = configuration.getStringValue(ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS),
        .keepalivesIdleSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC),
        .keepalivesIntervalSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC),
        .keepalivesCountSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_COUNT),
        .targetSessionAttrs = configuration.getStringValue(ConfigurationKey::POSTGRES_TARGET_SESSION_ATTRS),
        .useSsl = configuration.getBoolValue(ConfigurationKey::POSTGRES_USESSL),
        .serverRootCertPath = configuration.getStringValue(ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH),
        .sslCertificatePath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH),
        .sslKeyPath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_KEY_PATH),
    };
}
std::string PostgresConnection::readOnlyConnectString()
{
    using enum ConfigurationKey;
    const auto& configuration = Configuration::instance();
    auto readOnlyDBHost = configuration.getOptionalStringValue(POSTGRES_RO_HOST);
    Expect3(readOnlyDBHost.has_value(), "readOnlyDatabaseFactory is set, but readonly DB is not configured", std::logic_error);
    const auto strValue = [&configuration](ConfigurationKey key, ConfigurationKey fallbackKey){
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return *str;
        }
        return configuration.getStringValue(fallbackKey);
    };
    const auto strOptValue = [&configuration](ConfigurationKey key, ConfigurationKey fallbackKey){
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return str;
        }
        return configuration.getOptionalStringValue(fallbackKey);
    };
    const auto boolValue = [&configuration](ConfigurationKey key, ConfigurationKey fallbackKey){
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return String::toBool(*str);
        }
        return configuration.getBoolValue(fallbackKey);
    };
    return PostgresConnectString{
        .host = value(std::move(readOnlyDBHost)),
        .port = strValue(POSTGRES_RO_PORT, POSTGRES_PORT),
        .user = strValue(POSTGRES_RO_USER, POSTGRES_USER),
        .password = strValue(POSTGRES_RO_PASSWORD, POSTGRES_PASSWORD),
        .dbname = strValue(POSTGRES_RO_DATABASE, POSTGRES_DATABASE),
        .connectTimeout = strValue(POSTGRES_RO_CONNECT_TIMEOUT_SECONDS, POSTGRES_CONNECT_TIMEOUT_SECONDS),
        .enableScramAuthentication = boolValue(POSTGRES_RO_ENABLE_SCRAM_AUTHENTICATION, POSTGRES_ENABLE_SCRAM_AUTHENTICATION),
        .tcpUserTimeoutMs = strValue(POSTGRES_RO_TCP_USER_TIMEOUT_MS, POSTGRES_TCP_USER_TIMEOUT_MS),
        .keepalivesIdleSec = strValue(POSTGRES_RO_KEEPALIVES_IDLE_SEC, POSTGRES_KEEPALIVES_IDLE_SEC),
        .keepalivesIntervalSec = strValue(POSTGRES_RO_KEEPALIVES_INTERVAL_SEC, POSTGRES_KEEPALIVES_INTERVAL_SEC),
        .keepalivesCountSec = strValue(POSTGRES_RO_KEEPALIVES_COUNT, POSTGRES_KEEPALIVES_COUNT),
        .targetSessionAttrs = strValue(POSTGRES_RO_TARGET_SESSION_ATTRS, POSTGRES_TARGET_SESSION_ATTRS),
        .useSsl = boolValue(POSTGRES_RO_USESSL, POSTGRES_USESSL),
        .serverRootCertPath = strValue(POSTGRES_RO_SSL_ROOT_CERTIFICATE_PATH, POSTGRES_SSL_ROOT_CERTIFICATE_PATH),
        .sslCertificatePath = strOptValue(POSTGRES_RO_SSL_CERTIFICATE_PATH, POSTGRES_SSL_CERTIFICATE_PATH),
        .sslKeyPath = strOptValue(POSTGRES_RO_SSL_KEY_PATH, POSTGRES_SSL_KEY_PATH),
    };
}


void PostgresConnection::connectIfNeeded()
{
    if (! mConnection || ! mConnection->is_open())
    {
        TLOG(INFO) << "connecting to database";
        mConnectionInfo = {};
        mConnection = std::make_unique<pqxx::connection>(mConnectionString);
        mConnectionInfo = std::make_optional<DatabaseConnectionInfo>(
            {.dbname = mConnection->dbname(),
             .hostname = mConnection->hostname(),
             .port = mConnection->port(),
             .connectionTimestamp = model::Timestamp::now(),
             .maxAge = std::chrono::minutes{
                 Configuration::instance().getIntValue(ConfigurationKey::POSTGRES_CONNECTION_MAX_AGE_MINUTES)}});
        TVLOG(1) << "connected to " << toString(mConnectionInfo.value());
        mConnection->set_verbosity(pqxx::error_verbosity::normal);
        mConnection->set_notice_handler([](auto msg) {
            TVLOG(1) << "error/warning from postgres: " << msg;
        });
    }
}


void PostgresConnection::close()
{
    if (mConnectionInfo)
    {
        TVLOG(1) << "closing connection to database " << toString(*mConnectionInfo);
    }
    mConnection.reset();
    mConnectionInfo = {};
}


std::unique_ptr<pqxx::transaction_base> PostgresConnection::createTransaction(TransactionMode mode)
{
    std::unique_ptr<pqxx::transaction_base> transaction;
    TVLOG(2) << "transaction start";
    try
    {
        Expect3(mConnection, "connection to database not established", std::logic_error);
        transaction = createTransactionInternal(mode);
    }
    catch (const pqxx::broken_connection& brokenConnection)
    {
        TVLOG(1) << "caught pqxx::broken_connection: " << brokenConnection.what();
        if (mConnectionInfo)
        {
            TLOG(INFO) << "lost connection to " << toString(*mConnectionInfo);
        }
        close();
        connectIfNeeded();
        TVLOG(1) << "transaction start 2nd try";
        transaction = createTransactionInternal(mode);
    }
    TVLOG(2) << "transaction started";
    return transaction;
}

std::unique_ptr<pqxx::transaction_base> PostgresConnection::createTransactionInternal(TransactionMode mode)
{
    switch (mode)
    {
        case TransactionMode::autocommit:
            return std::make_unique<pqxx::nontransaction>(*mConnection);
        case TransactionMode::transaction:
            return std::make_unique<pqxx::work>(*mConnection);
    }
    Fail("unknown value for TransactionMode: " + std::to_string(static_cast<uintmax_t>(mode)));
}


PostgresConnection::operator pqxx::connection&() const
{
    Expect3(mConnection, "connection object is null", std::logic_error);
    return *mConnection;
}

std::optional<DatabaseConnectionInfo> PostgresConnection::getConnectionInfo() const
{
    return mConnectionInfo;
}

void PostgresConnection::recreateConnection()
{
    if (mConnectionInfo && connectionDuration(*mConnectionInfo) > mConnectionInfo->maxAge)
    {
        TVLOG(1) << "recreating connection to database";
        close();
        connectIfNeeded();
    }
}
