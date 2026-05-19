/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/PostgresConnectString.hxx"

#include "shared/util/TLog.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"

#include <boost/algorithm/string/replace.hpp>

std::string PostgresConnectString::str() &&
{
    auto sslmode = connectStringSslMode();
    std::string connectionString =
        "host='" + host + "' port='" + port + "' dbname='" + dbname + "' user='" + user + "'" +
        (password.empty() ? "" : " password='" + password + "'") + sslmode +
        (targetSessionAttrs.empty() ? "" : (" target_session_attrs=" + targetSessionAttrs)) +
        " connect_timeout=" + connectTimeout + " tcp_user_timeout=" + tcpUserTimeoutMs + " keepalives=1" +
        " keepalives_idle=" + keepalivesIdleSec + " keepalives_interval=" + keepalivesIntervalSec +
        " keepalives_count=" + keepalivesCountSec + (enableScramAuthentication ? " channel_binding=require" : "");

    JsonLog(LogId::INFO, JsonLog::makeVLogReceiver(0))
        .message("postgres connection string")
        .keyValue("value", boost::replace_all_copy(connectionString, password, "******"));
    TVLOG(2) << "using connection string '" << boost::replace_all_copy(connectionString, password, "******") << "'";
    return connectionString;
}


std::string PostgresConnectString::connectStringSslMode()
{
    if (serverRootCertPath.empty())
    {
        return (useSsl) ? (" sslmode=require") : ("");
    }
    std::string sslmode = " sslmode=verify-full sslrootcert='" + serverRootCertPath + "'";
    if (sslCertificatePath.has_value() && ! String::trim(sslCertificatePath.value()).empty() &&
        sslKeyPath.has_value() && ! String::trim(sslKeyPath.value()).empty())
    {
        const auto sslcert = String::trim(sslCertificatePath.value());
        const auto sslkey = String::trim(sslKeyPath.value());
        if (! (sslcert.empty() || sslkey.empty()))
        {
            sslmode += " sslcert='" + sslcert + "' sslkey='" + sslkey + "'";
        }
    }
    return sslmode;
}

PostgresConnectString::operator std::string() &&
{
    return std::move(*this).str();
}
