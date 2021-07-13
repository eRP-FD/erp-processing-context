#ifndef E_LIBRARY_SERVER_ERRORHANDLER_HXX
#define E_LIBRARY_SERVER_ERRORHANDLER_HXX

#include <boost/beast/http/error.hpp>

class ErrorHandler
{
public:
    boost::beast::error_code ec;

    explicit ErrorHandler (boost::beast::error_code ec = {});

    void throwOnServerError (const std::string& message);
    void reportServerError (const std::string& message);
};

#endif
