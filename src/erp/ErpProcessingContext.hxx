/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
