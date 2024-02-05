/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX

#include "erp/model/Timestamp.hxx"

#include <string>

struct DatabaseConnectionInfo {
    std::string dbname;
    std::string hostname;
    std::string port;
    model::Timestamp connectionTimestamp;
    model::Timestamp::duration_t maxAge;
};

model::Timestamp::duration_t connectionDuration(const DatabaseConnectionInfo& info);
std::string connectionDurationStr(const DatabaseConnectionInfo& info);
std::string toString(const DatabaseConnectionInfo& info);

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONINFO_HXX
