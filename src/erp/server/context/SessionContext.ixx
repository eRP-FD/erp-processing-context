#include "erp/server/context/SessionContext.hxx"

#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/pc/PcServiceContext.hxx"


template<class ServiceContextType>
SessionContext<ServiceContextType>::SessionContext (
    ServiceContextType& serviceContext_,
    ServerRequest& request_,
    ServerResponse& response_)
    : serviceContext(serviceContext_),
      request(request_),
      response(response_),
      accessLog()
{
}



