/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX

class RequestHandlerManager;

namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager);
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
