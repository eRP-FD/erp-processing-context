/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPPROCESSINGCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_ERPPROCESSINGCONTEXT_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.hxx"


class ErpProcessingContext
{
public:
    ErpProcessingContext (
        uint16_t port,
        std::shared_ptr<PcServiceContext> serviceContext,
        RequestHandlerManager<PcServiceContext>&& handlerManager);

    HttpsServer<PcServiceContext>& getServer (void);

    static void addPrimaryEndpoints (
        RequestHandlerManager<PcServiceContext>& primaryManager,
        RequestHandlerManager<PcServiceContext>&& secondaryManager = RequestHandlerManager<PcServiceContext>());
    static void addSecondaryEndpoints (
        RequestHandlerManager<PcServiceContext>& handlerManager);

private:
    HttpsServer<PcServiceContext> mServer;
};


#endif
