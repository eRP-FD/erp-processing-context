#ifndef ERP_PROCESSING_CONTEXT_RESPONSEVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_RESPONSEVALIDATOR_HXX


#include "erp/service/Operation.hxx"


class ServerResponse;

class ResponseValidator
{
public:
    static void validate (ServerResponse& response, Operation operation);
};


#endif
