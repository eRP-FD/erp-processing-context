/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_SERVEREXCEPTION_HXX
#define E_LIBRARY_UTIL_SERVEREXCEPTION_HXX

#include <string>
#include <stdexcept>

class ServerException
    : public std::runtime_error
{
public:
    explicit ServerException (const std::string& message);
};

#endif
