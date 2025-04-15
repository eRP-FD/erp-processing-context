/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#include "shared/database/DatabaseConnectionInfo.hxx"

#include <iomanip>

model::Timestamp::duration_t connectionDuration(const DatabaseConnectionInfo& info)
{
    return model::Timestamp::now() - info.connectionTimestamp;
}

std::string connectionDurationStr(const DatabaseConnectionInfo& info)
{
    auto hhMmSs = std::chrono::hh_mm_ss(connectionDuration(info));
    return (std::ostringstream{} << std::setw(2) << std::setfill('0') << hhMmSs.hours().count() << ":" << std::setw(2)
                                 << std::setfill('0') << hhMmSs.minutes().count() << ":" << std::setw(2)
                                 << std::setfill('0') << hhMmSs.seconds().count())
        .str();
}

std::string toString(const DatabaseConnectionInfo& info)
{
    auto res = info.hostname;
    res.append(":").append(info.port).append("/").append(info.dbname);
    using namespace std::chrono_literals;
    if (connectionDuration(info) > 1s)
    {
        res.append(" (connected for ").append(connectionDurationStr(info)).append(")");
    }
    return res;
}
