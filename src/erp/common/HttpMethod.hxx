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
