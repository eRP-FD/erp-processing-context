/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/context/SessionContext.hxx"

#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/pc/PcServiceContext.hxx"


template<class ServiceContextType>
SessionContext<ServiceContextType>::SessionContext (
    ServiceContextType& serviceContext_,
    ServerRequest& request_,
    ServerResponse& response_,
    AccessLog& accessLog_)
    : serviceContext(serviceContext_),
      request(request_),
      response(response_),
      accessLog(accessLog_)
{
}



