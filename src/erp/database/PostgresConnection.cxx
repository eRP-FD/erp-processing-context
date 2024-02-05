/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/PostgresConnection.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"


PostgresConnection::PostgresConnection(const std::string_view& connectionString)
    : mConnectionString(connectionString)
    , mConnection()
    , mErrorHandler()
{
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
