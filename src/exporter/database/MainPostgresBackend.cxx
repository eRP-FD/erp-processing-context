/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MainPostgresBackend.hxx"
#include "shared/util/DurationConsumer.hxx"

namespace
{
#define QUERY(name, query) const QueryDefinition name = {#name, query};

QUERY(healthCheckQuery, "SELECT FROM erp.task LIMIT 1")

#undef QUERY

}// anonymous namespace

using namespace exporter;

thread_local PostgresConnection MainPostgresBackend::mConnection{PostgresConnection::defaultConnectString()};


MainPostgresBackend::MainPostgresBackend()
    : CommonPostgresBackend(mConnection, PostgresConnection::defaultConnectString())
{
}

void MainPostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << healthCheckQuery.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:healthCheck");
    const auto result = mTransaction->exec(healthCheckQuery.query);
    TVLOG(2) << "got " << result.size() << " results";
}

PostgresConnection& MainPostgresBackend::connection() const
{
    return mConnection;
}
