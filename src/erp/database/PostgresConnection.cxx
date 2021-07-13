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
