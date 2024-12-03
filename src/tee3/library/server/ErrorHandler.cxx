/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/server/ErrorHandler.hxx"
#include "library/log/Logging.hxx"
#include "library/util/ServerException.hxx"

#include <boost/asio/ssl/error.hpp>

namespace epa
{

ErrorHandler::ErrorHandler(boost::beast::error_code ec)
  : ec(ec)
{
}


void ErrorHandler::throwOnServerError(const std::string& message) const
{
    switch (ec.value())
    {
        case boost::asio::ssl::error::stream_truncated:
            // This branch should eventually be removed. Special handling of error codes belongs
            // where the knowledge exists to separate "good" from "bad" error codes.
            // Fallthrough to avoid clang-tidy message.
        case 0:
            return;

        default:
            throw ServerException(message + ": " + ec.message());
    }
}


void ErrorHandler::reportServerError(const std::string& message) const
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

    switch (ec.value())
    {
        case boost::asio::ssl::error::stream_truncated:
        case 0:
            return;
        default:
            LOG(ERROR) << message << ": " << ec.message();
            break;
    }
}

} // namespace epa
