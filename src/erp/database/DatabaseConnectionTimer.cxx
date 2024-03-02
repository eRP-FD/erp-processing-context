/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseConnectionTimer.hxx"
#include "PostgresBackend.hxx"
#include "erp/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/util/TLog.hxx"

DatabaseConnectionTimerHandler::DatabaseConnectionTimerHandler(PcServiceContext& serviceContext,
                                                               std::chrono::steady_clock::duration interval)
    : FixedIntervalHandler(interval)
    , mThreadPool(serviceContext.getTeeServer().getThreadPool())
    , mServiceContext(serviceContext)
{
}

void DatabaseConnectionTimerHandler::timerHandler()
{
    TVLOG(1) << "checking/refreshing database connections";
    mThreadPool.runOnAllThreads([] {
        refreshConnection();
    });
    mServiceContext.getAdminServer().getThreadPool().runOnAllThreads([] {
        refreshConnection();
    });
    try
    {
        mServiceContext.getBlobCache()->recreateDatabaseConnection();
        mServiceContext.getVsdmKeyBlobDatabase().recreateConnection();
    }
    catch (const std::exception& ex)
    {
        TLOG(ERROR) << "exception during refresh of database connection: " << ex.what();
    }
    catch (...)
    {
        TLOG(ERROR) << "unknown exception during refresh of database connection";
    }
}

void DatabaseConnectionTimerHandler::refreshConnection()
{
    try
    {
        PostgresBackend::recreateConnection();
    }
    catch (const std::exception& ex)
    {
        TLOG(ERROR) << "exception during refresh of database connection: " << ex.what();
    }
    catch (...)
    {
        TLOG(ERROR) << "unknown exception during refresh of database connection";
    }
}
