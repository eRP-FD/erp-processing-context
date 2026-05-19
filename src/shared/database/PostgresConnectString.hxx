/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include <optional>
#include <string>

class PostgresConnectString
{
public:
    const std::string host;
    const std::string port;
    const std::string user;
    const std::string password;
    const std::string dbname;
    const std::string connectTimeout;
    bool enableScramAuthentication;
    const std::string tcpUserTimeoutMs;
    const std::string keepalivesIdleSec;
    const std::string keepalivesIntervalSec;
    const std::string keepalivesCountSec;
    const std::string targetSessionAttrs;
    const bool useSsl;
    const std::string serverRootCertPath;
    const std::optional<std::string> sslCertificatePath;
    const std::optional<std::string> sslKeyPath;

    std::string str() &&;
    operator std::string() &&;

private:
    std::string connectStringSslMode();
};
