/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/PostgresConnection.hxx"

#include "shared/database/PostgresConnectionParameters.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <pqxx/nontransaction>
#include <pqxx/transaction>

PostgresConnection::PostgresConnection(std::vector<PostgresConnectionParameters> connectionParameters)
    : mConnectionParameters(std::move(connectionParameters))
    , mConnection()
{
}

PostgresConnectionParameters PostgresConnection::defaultConnectParameters()
{
    const auto& configuration = Configuration::instance();
    return PostgresConnectionParameters{
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

std::vector<PostgresConnectionParameters> PostgresConnection::readOnlyConnectParameters()
{
    using enum ConfigurationKey;
    const auto& configuration = Configuration::instance();
    auto readOnlyDBHost = configuration.getOptionalStringValue(POSTGRES_RO_HOST);
    Expect3(readOnlyDBHost.has_value(), "readOnlyDatabaseFactory is set, but readonly DB is not configured",
            std::logic_error);

    const auto split = [](const std::string& str) {
        // Configuration class uses semicolon to separate strings and postgres uses comma - allow both:
        auto sepPos = str.find_first_of(";,");
        char sep = ';';
        if (sepPos != std::string::npos)
        {
            sep = str[sepPos];
        }
        return String::split(str, sep);
    };
    std::vector<std::string> hosts = split(*readOnlyDBHost);
    Expect3(! hosts.empty() && ! hosts[0].empty(), "readOnlyDatabaseFactory is set, but readonly DB is not configured",
            std::logic_error);
    const auto strForIdx = [&](const std::string& str, ConfigurationKey usedKey, size_t idx) {
        auto splitStr = split(str);
        Expect3(splitStr.size() == 1 || splitStr.size() == hosts.size(),
                fmt::format("entries in {} must be 1 or match number of configured hosts({}): {}",
                            magic_enum::enum_name(usedKey), hosts.size(), str),
                std::logic_error);
        return String::trim(splitStr[std::min(splitStr.size() - 1, idx)]);
    };
    const auto strValue = [&](ConfigurationKey key, ConfigurationKey fallbackKey, size_t idx) {
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return strForIdx(value(str), key, idx);
        }
        return strForIdx(configuration.getStringValue(fallbackKey), fallbackKey, idx);
    };
    const auto strOptValue = [&](ConfigurationKey key, ConfigurationKey fallbackKey,
                                 size_t idx) -> std::optional<std::string> {
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return strForIdx(value(str), key, idx);
        }
        if (auto str = configuration.getOptionalStringValue(fallbackKey))
        {
            return strForIdx(value(str), fallbackKey, idx);
        }
        return std::nullopt;
    };
    const auto boolValue = [&](ConfigurationKey key, ConfigurationKey fallbackKey, size_t idx) {
        if (auto str = configuration.getOptionalStringValue(key))
        {
            return String::toBool(strForIdx(value(str), key, idx));
        }
        return String::toBool(strForIdx(configuration.getStringValue(fallbackKey), fallbackKey, idx));
    };
    std::vector<PostgresConnectionParameters> result;
    result.reserve(hosts.size());
    for (size_t i = 0; i < hosts.size(); ++i)
    {

        result.emplace_back(PostgresConnectionParameters{
            .host = String::trim(hosts[i]),
            .port = strValue(POSTGRES_RO_PORT, POSTGRES_PORT, i),
            .user = strValue(POSTGRES_RO_USER, POSTGRES_USER, i),
            .password = strValue(POSTGRES_RO_PASSWORD, POSTGRES_PASSWORD, i),
            .dbname = strValue(POSTGRES_RO_DATABASE, POSTGRES_DATABASE, i),
            .connectTimeout = strValue(POSTGRES_RO_CONNECT_TIMEOUT_SECONDS, POSTGRES_CONNECT_TIMEOUT_SECONDS, i),
            .enableScramAuthentication =
                boolValue(POSTGRES_RO_ENABLE_SCRAM_AUTHENTICATION, POSTGRES_ENABLE_SCRAM_AUTHENTICATION, i),
            .tcpUserTimeoutMs = strValue(POSTGRES_RO_TCP_USER_TIMEOUT_MS, POSTGRES_TCP_USER_TIMEOUT_MS, i),
            .keepalivesIdleSec = strValue(POSTGRES_RO_KEEPALIVES_IDLE_SEC, POSTGRES_KEEPALIVES_IDLE_SEC, i),
            .keepalivesIntervalSec = strValue(POSTGRES_RO_KEEPALIVES_INTERVAL_SEC, POSTGRES_KEEPALIVES_INTERVAL_SEC, i),
            .keepalivesCountSec = strValue(POSTGRES_RO_KEEPALIVES_COUNT, POSTGRES_KEEPALIVES_COUNT, i),
            .targetSessionAttrs = strValue(POSTGRES_RO_TARGET_SESSION_ATTRS, POSTGRES_TARGET_SESSION_ATTRS, i),
            .useSsl = boolValue(POSTGRES_RO_USESSL, POSTGRES_USESSL, i),
            .serverRootCertPath =
                strValue(POSTGRES_RO_SSL_ROOT_CERTIFICATE_PATH, POSTGRES_SSL_ROOT_CERTIFICATE_PATH, i),
            .sslCertificatePath = strOptValue(POSTGRES_RO_SSL_CERTIFICATE_PATH, POSTGRES_SSL_CERTIFICATE_PATH, i),
            .sslKeyPath = strOptValue(POSTGRES_RO_SSL_KEY_PATH, POSTGRES_SSL_KEY_PATH, i),
        });
    }
    return result;
}


void PostgresConnection::connect(const PostgresConnectionParameters& connectionParameters)
{
    TLOG(INFO) << "connecting to database";
    mConnectionInfo = {};
    mConnection = std::make_unique<pqxx::connection>(connectionParameters.str());
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

void PostgresConnection::connectIfNeeded()
{

    if (mConnection && mConnection->is_open())
    {
        return;
    }
    if (mConnectionParameters.size() == 1)
    {
        connect(mConnectionParameters[0]);
        return;
    }
    connectMulti();
}

bool PostgresConnection::isPreferReadOnly() const
{
    using namespace std::string_view_literals;
    static const std::set preferReadOnlyAttrs = {"read-only"sv, "standby"sv, "prefer-standby"sv};
    return std::ranges::any_of(mConnectionParameters, [&](const PostgresConnectionParameters& p) {
        return preferReadOnlyAttrs.contains(p.targetSessionAttrs);
    });
}

void PostgresConnection::connectMulti()
{
    // ERP-36190
    std::unique_ptr<pqxx::connection> fallbackConnection;
    std::optional<DatabaseConnectionInfo> fallbackInfo;

    bool preferReadOnly = isPreferReadOnly();

    for (auto params : mConnectionParameters)
    {
        params.targetSessionAttrs.clear();
        try {
            connect(params);
            auto tx = createTransactionInternal(TransactionMode::autocommit);
            auto result = tx->exec("SHOW transaction_read_only").one_field().view();
            bool isReadOnly = (result == "on");
            if (isReadOnly == preferReadOnly)
            {
                return;
            }
            if (!isReadOnly && ! fallbackConnection)
            {
                fallbackConnection = std::move(mConnection);
                fallbackInfo = mConnectionInfo;
            }
            continue;
        }
        catch (const pqxx::broken_connection&)
        {
            TLOG(INFO) << "postgres connection to " << params.host << ":" << params.port << " db: " << params.dbname
                       << " failed - tying next";
        }
    }
    Expect(fallbackConnection != nullptr, "No suitable connection found.");
    mConnection = std::move(fallbackConnection);
    mConnectionInfo = std::move(fallbackInfo);
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
