/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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

MainPostgresBackend::MainPostgresBackend()
    : CommonPostgresBackend(threadConnection())
{
}

void MainPostgresBackend::healthCheck()
{
    checkCommonPreconditions();
    TVLOG(2) << healthCheckQuery.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "healthcheck");
    const auto result = transaction()->exec(healthCheckQuery.query);
    TVLOG(2) << "got " << result.size() << " results";
}

PostgresConnection& MainPostgresBackend::connection() const
{
    return threadConnection();
}

PostgresConnection& exporter::MainPostgresBackend::threadConnection()
{
    static thread_local PostgresConnection connection{PostgresConnection::defaultConnectString()};
    return connection;
}
