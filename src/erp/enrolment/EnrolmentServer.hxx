/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX


#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/server/HttpsServer.hxx"

namespace EnrolmentServer
{
constexpr uint16_t DefaultEnrolmentServerPort = 9191;
void addEndpoints (RequestHandlerManager& handlers);
}


#endif
