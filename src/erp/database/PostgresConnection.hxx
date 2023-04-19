/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX

#include "erp/database/DatabaseConnectionInfo.hxx"

#include <memory>
#include <pqxx/connection>
#include <pqxx/transaction>


class PostgresConnection
{
public:
    explicit PostgresConnection(const std::string_view& connectionString);

    /// @brief (re-) connects if not already connected. Should not be called in the middle of a transaction.
    void connectIfNeeded();
    void close();
    std::unique_ptr<pqxx::work> createTransaction();

    operator pqxx::connection&() const;// NOLINT(google-explicit-constructor)

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const;

private:
    std::string mConnectionString;
    std::unique_ptr<pqxx::connection> mConnection;

    class ErrorHandler : public pqxx::errorhandler
    {
    public:
        using pqxx::errorhandler::errorhandler;
        bool operator()(const char* msg) noexcept override;
    };
    std::unique_ptr<ErrorHandler> mErrorHandler;
    std::optional<DatabaseConnectionInfo> mConnectionInfo;
};


#endif//ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
