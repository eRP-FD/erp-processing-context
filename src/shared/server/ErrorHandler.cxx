/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/ErrorHandler.hxx"

#include "shared/util/ServerException.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/ssl/error.hpp>

#include <sstream>

ErrorHandler::ErrorHandler (boost::beast::error_code ec_)
    : ec(ec_)
{
}


void ErrorHandler::throwOnServerError (const std::string& message, const FileNameAndLineNumber& fileAndLineNumber) const
{
    switch(ec.value())
    {
        case 0:
            return;

        default:
            std::stringstream s;
            s << message << " : " << ec.message();
            throw ExceptionWrapper<ServerException>::create(fileAndLineNumber, s.str());
    }
}


void ErrorHandler::reportServerError (const std::string& message) const
{
    // ssl::error::stream_truncated, also known as an SSL "short read",
    // indicates the peer closed the connection without performing the
    // required closing handshake (for example, Google does this to
    // improve performance). Generally this can be a security issue,
    // but if your communication protocol is self-terminated (as
    // it is with both HTTP and WebSocket) then you may simply
    // ignore the lack of close_notify.
    //
    // https://github.com/boostorg/beast/issues/38
    //
    // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
    //
    // When a short read would cut off the end of an HTTP message,
    // Beast returns the error beast::http::error::partial_message.
    // Therefore, if we see a short read here, it has occurred
    // after the message has been completed, so it is safe to ignore it.

    if (ec.failed() && ec != boost::asio::ssl::error::stream_truncated)
    {
        TLOG(ERROR) << message.c_str() << ": " << ec.message().c_str();
    }
}
