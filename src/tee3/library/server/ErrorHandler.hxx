/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_SERVER_ERRORHANDLER_HXX
#define EPA_LIBRARY_SERVER_ERRORHANDLER_HXX

#include <boost/beast/http/error.hpp>

namespace epa
{

class ErrorHandler
{
public:
    boost::beast::error_code ec;

    explicit ErrorHandler(boost::beast::error_code ec = {});

    void throwOnServerError(const std::string& message) const;
    void reportServerError(const std::string& message) const;
};
} // namespace epa
#endif
