/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "fhirtools/model/NumberAsStringParserDocument.hxx"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/status.hpp>

#include <string>
#include <string_view>


namespace erp_fhir_ws
{
class ErpFhirWsException : public std::runtime_error {
    using  runtime_error::runtime_error;
};

class ErpFhirWS : public std::enable_shared_from_this<ErpFhirWS>
{
public:
    static std::shared_ptr<ErpFhirWS> create(boost::asio::any_io_executor exec);

    void start(size_t workers);
    void stop();

    virtual ~ErpFhirWS();

private:
    using BoostSocket = boost::asio::ip::tcp::socket;
    boost::asio::awaitable<void> acceptloop();

    static void views(BoostSocket& socket);
    static void validate(BoostSocket& socket, const std::string& query, const std::string& body);
    static void servePublic(BoostSocket& socket, const std::string& path);

    static void respond(BoostSocket& socket, std::string responseBody, std::string_view mimeType = "application/json",
                 boost::beast::http::status status = boost::beast::http::status::ok);
    static void badRequest(BoostSocket& socket, std::string_view message);
    static void internalServerError(BoostSocket& socket);

    explicit ErpFhirWS(boost::asio::any_io_executor exec);

    boost::asio::any_io_executor mIoExecutor;
    boost::asio::ip::tcp::acceptor mAcceptor;
    std::atomic_bool mRunning = false;
};


}// namespace erp_fhir_ws
