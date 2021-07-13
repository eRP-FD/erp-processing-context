#ifndef ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX

#include "erp/service/AuditDataCollector.hxx"
#include "erp/database/Database.hxx"
#include "erp/server/AccessLog.hxx"

#include <boost/core/noncopyable.hpp>
#include <functional>
#include <memory>
#include <string>


class ServerRequest;
class ServerResponse;
class PcServiceContext;


/**
 * A session context is a bag of values that are only accessible, with one exception, only from a single thread
 * and visible only to a single request handler.
 * The exception is a reference to the, effectively global, ServiceContext.
 */
template<class ServiceContextType>
class SessionContext : private boost::noncopyable
{
public:
    SessionContext(ServiceContextType& serviceContext, ServerRequest& request, ServerResponse& response);

    ServiceContextType& serviceContext;
    ServerRequest& request;
    ServerResponse& response;
    AccessLog accessLog;
};

template <>
class SessionContext<PcServiceContext>
{
public:
    SessionContext(PcServiceContext& serviceContext, ServerRequest& request, ServerResponse& response);

    PcServiceContext& serviceContext;
    ServerRequest& request;
    ServerResponse& response;
    AccessLog accessLog;
    bool callerWantsJson;

    AuditDataCollector& auditDataCollector();
    Database* database();
    std::unique_ptr<Database> releaseDatabase();
private:
    std::unique_ptr<AuditDataCollector> mAuditDataCollector;
    std::unique_ptr<Database> mDatabase;
};


#endif
