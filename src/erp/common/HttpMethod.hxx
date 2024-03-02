/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_COMMON_HTTPMETHOD_HXX
#define E_LIBRARY_COMMON_HTTPMETHOD_HXX

#include <sstream>
#include <string>

#ifdef DELETE
#undef DELETE
#endif

enum class HttpMethod
{
    UNKNOWN = 0, // used for responses

    DELETE,
    GET,
    HEAD,
    PATCH,
    POST,
    PUT
};

std::string toString (HttpMethod method);

std::ostream& operator<< (std::ostream& out, HttpMethod method);

#endif
