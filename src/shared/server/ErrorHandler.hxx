/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_SERVER_ERRORHANDLER_HXX
#define E_LIBRARY_SERVER_ERRORHANDLER_HXX

#include "shared/util/ExceptionWrapper.hxx"

#include <boost/beast/http/error.hpp>

class ErrorHandler
{
public:
    boost::beast::error_code ec;

    explicit ErrorHandler (boost::beast::error_code ec = {});

    void throwOnServerError (const std::string& message, const FileNameAndLineNumber& fileAndLineNumber) const;
    void reportServerError (const std::string& message) const;
};

#endif
