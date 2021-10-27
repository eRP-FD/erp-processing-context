/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmException.hxx"


HsmException::HsmException (const std::string& what_, const int errorCode_)
    : std::runtime_error(what_),
      errorCode(errorCode_)
{
}

