/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX

#include <string>

struct DatabaseConnectionInfo {
    std::string dbname;
    std::string hostname;
    std::string port;
};

inline std::string toString(const DatabaseConnectionInfo& info)
{
    auto res = info.hostname;
    return res.append(":").append(info.port).append("/").append(info.dbname);
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX
