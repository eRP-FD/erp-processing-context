/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/PostgresConnection.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <boost/algorithm/string.hpp>

PostgresConnection::PostgresConnection(const std::string_view& connectionString)
    : mConnectionString(connectionString)
    , mConnection()
    , mErrorHandler()
{
}

void PostgresConnection::setConnectionString(const std::string_view& connectionString)
{
    mConnectionString = connectionString;
}

std::string PostgresConnection::defaultConnectString()
{
    const auto& configuration = Configuration::instance();

    const std::string host = configuration.getStringValue(ConfigurationKey::POSTGRES_HOST);
    const std::string port = configuration.getStringValue(ConfigurationKey::POSTGRES_PORT);
    const std::string user = configuration.getStringValue(ConfigurationKey::POSTGRES_USER);
    const std::string password = configuration.getStringValue(ConfigurationKey::POSTGRES_PASSWORD);
    const std::string dbname = configuration.getStringValue(ConfigurationKey::POSTGRES_DATABASE);
    const std::string connectTimeout = configuration.getStringValue(ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS);
    bool enableScramAuthentication = configuration.getBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION);
    const std::string tcpUserTimeoutMs = configuration.getStringValue(ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS);
    const std::string keepalivesIdleSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC);
    const std::string keepalivesIntervalSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC);
    const std::string keepalivesCountSec = configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_COUNT);
    const std::string targetSessionAttrs = configuration.getStringValue(ConfigurationKey::POSTGRES_TARGET_SESSION_ATTRS);

    std::string sslmode = connectStringSslMode();

    std::string connectionString = "host='" + host
    + "' port='" + port
    + "' dbname='" + dbname
    + "' user='" + user + "'"
    + (password.empty() ? "" : " password='" + password + "'")
    + sslmode
    + (targetSessionAttrs.empty() ? "" : (" target_session_attrs=" + targetSessionAttrs))
    + " connect_timeout=" + connectTimeout
    + " tcp_user_timeout=" + tcpUserTimeoutMs
    + " keepalives=1"
    + " keepalives_idle=" + keepalivesIdleSec
    + " keepalives_interval=" + keepalivesIntervalSec
    + " keepalives_count=" + keepalivesCountSec
    + (enableScramAuthentication ? " channel_binding=require" : "");

    JsonLog(LogId::INFO, JsonLog::makeVLogReceiver(0))
    .message("postgres connection string")
    .keyValue("value", boost::replace_all_copy(connectionString, password, "******"));
    TVLOG(2) << "using connection string '" << boost::replace_all_copy(connectionString, password, "******") << "'";

    return connectionString;
}


std::string PostgresConnection::connectStringSslMode()
{
    const auto& configuration = Configuration::instance();

    const bool useSsl = configuration.getBoolValue(ConfigurationKey::POSTGRES_USESSL);
    const std::string serverRootCertPath = configuration.getStringValue(ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH);
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


void PostgresConnection::connectIfNeeded()
{
    if (! mConnection || ! mConnection->is_open())
    {
        TVLOG(1) << "connecting to database";
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
        mErrorHandler = std::make_unique<ErrorHandler>(*mConnection);
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


std::unique_ptr<pqxx::work> PostgresConnection::createTransaction()
{
    std::unique_ptr<pqxx::work> transaction;
    TVLOG(1) << "transaction start";
    try
    {
        Expect3(mConnection, "connection to database not established", std::logic_error);
        transaction = std::make_unique<pqxx::work>(*mConnection);
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
        transaction = std::make_unique<pqxx::work>(*mConnection);
    }
    TVLOG(1) << "transaction started";
    return transaction;
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


bool PostgresConnection::ErrorHandler::operator()(const char* msg) noexcept
{
    TVLOG(1) << "error/warning from postgres: " << msg;
    // true: other error handlers shall be called after this one.
    return true;
}
