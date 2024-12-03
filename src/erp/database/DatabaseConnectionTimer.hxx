/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
*
* non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONTIMER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONTIMER_HXX

#include "shared/util/PeriodicTimer.hxx"

class DatabaseBackend;
class ThreadPool;
class PcServiceContext;

class DatabaseConnectionTimerHandler : public FixedIntervalHandler
{
public:
    explicit DatabaseConnectionTimerHandler(PcServiceContext& serviceContext, std::chrono::steady_clock::duration interval);

private:
    void timerHandler() override;
    static void refreshConnection();

    ThreadPool& mThreadPool;
    PcServiceContext& mServiceContext;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_DATABASECONNECTIONTIMER_HXX
