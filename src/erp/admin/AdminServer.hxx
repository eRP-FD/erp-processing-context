/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX

#include "erp/server/HttpsServer.hxx"
#include "erp/server/ServerSocketHandler.hxx"
#include "erp/server/context/SessionContext.hxx"

#include <cstdint>
#include <memory>

class AdminServiceContext;
template<class ServiceContextType>
class RequestHandlerManager;

class AdminServer
{
public:
    AdminServer(const std::string_view& adminInterface, const uint16_t adminPort,
                RequestHandlerManager<AdminServiceContext>&& handlerManager,
                std::unique_ptr<AdminServiceContext> serviceContext);

    static void addEndpoints(RequestHandlerManager<AdminServiceContext>& manager);

    HttpsServer<AdminServiceContext>& getServer(void);

private:
    HttpsServer<AdminServiceContext> mServer;
};

extern template class HttpsServer<AdminServiceContext>;
extern template class RequestHandlerContainer<AdminServiceContext>;
extern template class RequestHandlerContext<AdminServiceContext>;
extern template class RequestHandlerManager<AdminServiceContext>;
extern template class ServerSocketHandler<AdminServiceContext>;
extern template class SessionContext<AdminServiceContext>;

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
