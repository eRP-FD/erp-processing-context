#include "erp/server/handler/RequestHandlerInterface.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"


template class RequestHandlerInterface<PcServiceContext>;
template class RequestHandlerInterface<EnrolmentServiceContext>;
