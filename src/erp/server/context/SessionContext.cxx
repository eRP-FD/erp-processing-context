/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/context/SessionContext.hxx"
#include "erp/pc/PcServiceContext.hxx"

SessionContext::SessionContext(
    PcServiceContext& serviceContext,
    ServerRequest& request,
    ServerResponse& response,
    AccessLog& log,
    model::Timestamp initSessionTime)
    : serviceContext(serviceContext)
    , request(request)
    , response(response)
    , accessLog(log)
    , callerWantsJson(false)
    , mSessionTime(initSessionTime)
{
}


AuditDataCollector& SessionContext::auditDataCollector()
{
    if(!mAuditDataCollector)
        mAuditDataCollector = std::make_unique<AuditDataCollector>();
    return *mAuditDataCollector;
}


Database* SessionContext::database()
{
    if (! mDatabase)
        mDatabase = serviceContext.databaseFactory();
    return mDatabase.get();
}

std::unique_ptr<Database> SessionContext::releaseDatabase()
{
    return std::move(mDatabase);
}

const model::Timestamp& SessionContext::sessionTime() const
{
    return mSessionTime;
}
