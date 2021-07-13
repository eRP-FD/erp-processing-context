#include "erp/hsm/HsmException.hxx"


HsmException::HsmException (const std::string& what_, const int errorCode_)
    : std::runtime_error(what_),
      errorCode(errorCode_)
{
}

