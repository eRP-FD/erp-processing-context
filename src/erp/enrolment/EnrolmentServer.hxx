/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX


#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/server/HttpsServer.hxx"

class EnrolmentServiceContext;

class EnrolmentServer
{
public:
    EnrolmentServer(
        const uint16_t enrolmentPort,
        RequestHandlerManager<EnrolmentServiceContext>&& handlerManager,
        std::unique_ptr<EnrolmentServiceContext> serviceContext);

    static void addEndpoints (RequestHandlerManager<EnrolmentServiceContext>& manager);

    HttpsServer<EnrolmentServiceContext>& getServer (void);

private:
    HttpsServer<EnrolmentServiceContext> mServer;
};


#endif
