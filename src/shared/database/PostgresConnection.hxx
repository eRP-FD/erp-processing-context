/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX

#include "shared/database/DatabaseConnectionInfo.hxx"

#include <memory>
#if defined (__GNUC__) && __GNUC__ == 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <pqxx/connection>
#include <pqxx/transaction>
#pragma GCC diagnostic pop
#else
#include <pqxx/connection>
#include <pqxx/transaction>
#endif

class PostgresConnection
{
public:
    explicit PostgresConnection(const std::string_view& connectionString);

    [[nodiscard]]
    static std::string defaultConnectString();

    [[nodiscard]]
    static std::string connectStringSslMode();

    /// @brief (re-) connects if not already connected. Should not be called in the middle of a transaction.
    void connectIfNeeded();
    void close();
    std::unique_ptr<pqxx::work> createTransaction();

    operator pqxx::connection&() const;// NOLINT(google-explicit-constructor)

    void setConnectionString(const std::string_view& connectionString);

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const;

    void recreateConnection();

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
