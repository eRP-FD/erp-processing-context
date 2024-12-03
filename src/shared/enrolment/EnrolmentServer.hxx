/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTSERVER_HXX


#include "shared/server/handler/RequestHandlerManager.hxx"

namespace EnrolmentServer
{
constexpr uint16_t DefaultEnrolmentServerPort = 9191;
void addEndpoints(RequestHandlerManager& handlers);
}


#endif
