/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include <optional>
#include <string>

class PostgresConnectionParameters
{
public:
    std::string host;
    std::string port;
    std::string user;
    std::string password;
    std::string dbname;
    std::string connectTimeout;
    bool enableScramAuthentication;
    std::string tcpUserTimeoutMs;
    std::string keepalivesIdleSec;
    std::string keepalivesIntervalSec;
    std::string keepalivesCountSec;
    std::string targetSessionAttrs;
    bool useSsl;
    std::string serverRootCertPath;
    std::optional<std::string> sslCertificatePath;
    std::optional<std::string> sslKeyPath;

    std::string str() const;

private:
    std::string connectStringSslMode() const;
};
