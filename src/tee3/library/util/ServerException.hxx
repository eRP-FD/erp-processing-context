/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_SERVEREXCEPTION_HXX
#define EPA_LIBRARY_UTIL_SERVEREXCEPTION_HXX

#include <stdexcept>
#include <string>

namespace epa
{

class ServerException : public std::runtime_error
{
public:
    explicit ServerException(const std::string& message);
};

} // namespace epa

#endif
