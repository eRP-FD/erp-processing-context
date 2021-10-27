/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_SERVER_ERRORHANDLER_HXX
#define E_LIBRARY_SERVER_ERRORHANDLER_HXX

#include "erp/util/ExceptionWrapper.hxx"

#include <boost/beast/http/error.hpp>

class ErrorHandler
{
public:
    boost::beast::error_code ec;

    explicit ErrorHandler (boost::beast::error_code ec = {});

    void throwOnServerError (const std::string& message, std::optional<FileNameAndLineNumber> fileAndLineNumber = std::nullopt);
    void reportServerError (const std::string& message);
};

#endif
