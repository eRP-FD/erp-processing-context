/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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
