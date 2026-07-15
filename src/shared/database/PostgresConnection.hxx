/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX

#include "shared/database/DatabaseConnectionInfo.hxx"
#include "shared/database/PostgresConnectionParameters.hxx"
#include "shared/database/TransactionMode.hxx"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#if defined (__GNUC__) && __GNUC__ == 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <pqxx/connection>
#include <pqxx/transaction_base>
#pragma GCC diagnostic pop
#else
#include <pqxx/connection>
#include <pqxx/transaction_base>
#endif

class PostgresConnection
{
public:
    explicit PostgresConnection(std::vector<PostgresConnectionParameters> connectionParameters);

    [[nodiscard]]
    static PostgresConnectionParameters defaultConnectParameters();
    [[nodiscard]]
    static std::vector<PostgresConnectionParameters> readOnlyConnectParameters();

    /// @brief (re-) connects if not already connected. Should not be called in the middle of a transaction.
    void connectIfNeeded();
    void close();
    std::unique_ptr<pqxx::transaction_base> createTransaction(TransactionMode mode = TransactionMode::transaction);

    operator pqxx::connection&() const;// NOLINT(google-explicit-constructor)

    std::optional<DatabaseConnectionInfo> getConnectionInfo() const;

    void recreateConnection();

    bool isPreferReadOnly() const;

private:
    void connect(const PostgresConnectionParameters& connectionParameters);
    void connectMulti();

    std::unique_ptr<pqxx::transaction_base> createTransactionInternal(TransactionMode mode);

    std::vector<PostgresConnectionParameters> mConnectionParameters;
    std::unique_ptr<pqxx::connection> mConnection;
    std::optional<DatabaseConnectionInfo> mConnectionInfo;
};


#endif//ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
