#include "erp/util/ServerException.hxx"

ServerException::ServerException (const std::string& message)
    : std::runtime_error(message.c_str())
{
}
