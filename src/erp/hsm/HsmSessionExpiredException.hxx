/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMSESSIONEXPIREDEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_HSMSESSIONEXPIREDEXCEPTION_HXX

#include "erp/hsm/HsmException.hxx"


/**
 * The exception does not carry any information beyond its name/type.
 */
class HsmSessionExpiredException : public HsmException
{
public:
    HsmSessionExpiredException (const std::string& what, int errorCode);
};


#endif
