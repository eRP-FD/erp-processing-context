/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ServerException.hxx"

ServerException::ServerException (const std::string& message)
    : std::runtime_error(message.c_str())
{
}
