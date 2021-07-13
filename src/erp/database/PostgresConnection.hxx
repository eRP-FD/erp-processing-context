#ifndef ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX

#include <memory>
#include <pqxx/connection>

class PostgresConnection
{
public:
    explicit PostgresConnection(const std::string_view& connectionString);

    /// @brief (re-) connects if not already connected. Should not be called in the middle of a transaction.
    void connectIfNeeded();
    void close();

    operator pqxx::connection&() const;// NOLINT(google-explicit-constructor)

private:
    const std::string mConnectionString;
    std::unique_ptr<pqxx::connection> mConnection;

    class ErrorHandler : public pqxx::errorhandler
    {
    public:
        using pqxx::errorhandler::errorhandler;
        bool operator()(const char* msg) noexcept override;
    };
    std::unique_ptr<ErrorHandler> mErrorHandler;
};


#endif//ERP_PROCESSING_CONTEXT_POSTGRESCONNECTION_HXX
