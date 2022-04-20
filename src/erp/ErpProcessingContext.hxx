/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPPROCESSINGCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_ERPPROCESSINGCONTEXT_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.hxx"

namespace ErpProcessingContext
{
void addPrimaryEndpoints (
    RequestHandlerManager& primaryManager,
    RequestHandlerManager&& secondaryManager = RequestHandlerManager());
void addSecondaryEndpoints (
    RequestHandlerManager& handlerManager);
}

#endif
