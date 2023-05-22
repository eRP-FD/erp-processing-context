/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX

class RequestHandlerManager;

namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager);
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
