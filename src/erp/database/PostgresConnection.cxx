/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/PostgresConnection.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"


PostgresConnection::PostgresConnection(const std::string_view& connectionString)
    : mConnectionString(connectionString)
    , mConnection()
    , mErrorHandler()
{
    connectIfNeeded();
}


void PostgresConnection::connectIfNeeded()
{
    if (! mConnection || ! mConnection->is_open())
    {
        TVLOG(1) << "connecting to database";
        mConnection = std::make_unique<pqxx::connection>(mConnectionString);
        mErrorHandler = std::make_unique<ErrorHandler>(*mConnection);
    }
}


void PostgresConnection::close()
{
    TVLOG(1) << "closing connection to database";
    mConnection.reset();
}


std::unique_ptr<pqxx::work> PostgresConnection::createTransaction()
{
    std::unique_ptr<pqxx::work> transaction;
    TVLOG(1) << "transaction start";
    try
    {
        transaction = std::make_unique<pqxx::work>(*mConnection);
    }
    catch (const pqxx::broken_connection& brokenConnection)
    {
        TVLOG(1) << "caught pqxx::broken_connection: " << brokenConnection.what();
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


bool PostgresConnection::ErrorHandler::operator()(const char* msg) noexcept
{
    TVLOG(1) << "error/warning from postgres: " << msg;
    // true: other error handlers shall be called after this one.
    return true;
}
