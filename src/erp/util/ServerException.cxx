/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ServerException.hxx"

ServerException::ServerException (const std::string& message)
    : std::runtime_error(message.c_str())
{
}
