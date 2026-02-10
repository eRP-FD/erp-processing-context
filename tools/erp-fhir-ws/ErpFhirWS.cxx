/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ErpFhirWS.hxx"

#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/GLogConfiguration.hxx"
#include "shared/util/UrlHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "WsValidateHandler.hxx"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sstream>

namespace erp_fhir_ws
{


std::shared_ptr<ErpFhirWS> erp_fhir_ws::ErpFhirWS::create(boost::asio::any_io_executor exec)
{
    struct Create : public ErpFhirWS {
        Create(boost::asio::any_io_executor exec)
            : ErpFhirWS{std::move(exec)}
        {
        }
    };
    return std::make_shared<Create>(std::move(exec));
}

erp_fhir_ws::ErpFhirWS::ErpFhirWS(boost::asio::any_io_executor exec)
    : mIoExecutor(std::move(exec))
    , mAcceptor(mIoExecutor)
{
}

erp_fhir_ws::ErpFhirWS::~ErpFhirWS() = default;

void erp_fhir_ws::ErpFhirWS::start(size_t workers)
{
    const auto& config = Configuration::instance();
    Expect(workers > 0, "Must have at least one worker.");
    auto addr = boost::asio::ip::make_address(config.serverHost());
    boost::asio::ip::tcp::endpoint endpoint{addr, config.serverPort()};
    boost::system::error_code ec;
    mAcceptor.open(endpoint.protocol());
    mAcceptor.set_option(boost::asio::socket_base::reuse_address{true});
    mAcceptor.bind(endpoint, ec);
    if (ec.failed())
    {
        TLOG(ERROR) << "Server socket bind error: " << ec.message();
        throw boost::system::system_error{ec};
    }
    mAcceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec.failed())
    {
        TLOG(ERROR) << "Server socket listen error: " << ec.message();
        throw boost::system::system_error{ec};
    }
    mRunning = true;
    for (size_t idx = 0; idx < workers; ++idx)
    {
        co_spawn(mIoExecutor, acceptloop(), consign(boost::asio::detached, shared_from_this(), make_work_guard(mIoExecutor)));
    }
    TLOG(INFO) << "Ready to accept commections on http://" << endpoint;
}

void erp_fhir_ws::ErpFhirWS::stop()
{
    mRunning = false;
}



boost::asio::awaitable<void> ErpFhirWS::acceptloop()
{
    while (mRunning)
    {
        auto [ec, socket] = co_await mAcceptor.async_accept(mIoExecutor, boost::asio::as_tuple(boost::asio::deferred));
        if (ec.failed())
        {
            TVLOG(0) << "accept failure: " << ec.message();
            continue;
        }
        std::string bufferStr;
        boost::asio::dynamic_string_buffer buffer{bufferStr};
        boost::beast::http::request<boost::beast::http::string_body> request;
        read(socket, buffer, request, ec);
        if (ec.failed())
        {
            TVLOG(0) << "error in request from " << socket.remote_endpoint() << ": " << ec.message();
            continue;
        }
        TVLOG(0) << "handling request from " << socket.remote_endpoint()<< ": " << request.method() << " " << request.target();
        auto [path, query, fragment] = UrlHelper::splitTarget(request.target());
        if (path == "/views" && request.method() == boost::beast::http::verb::get)
        {
            views(socket);
            continue;
        }
        if (path == "/validate" && request.method() == boost::beast::http::verb::post)
        {
            validate(socket, query, request.body());
            continue;
        }
        if (request.method() == boost::beast::http::verb::post)
        {
            badRequest(socket, "invalid method");
            continue;
        }
        servePublic(socket, path);
    }
}

void ErpFhirWS::views(BoostSocket& socket)
{
    const auto& config = Configuration::instance();
    rapidjson::Document doc{rapidjson::kArrayType};
    auto viewList = doc.GetArray();
    for (const auto& viewCfg = config.fhirResourceViewConfiguration<Configuration::ERP>();
         const auto& view : viewCfg.allViews())
    {
        viewList.PushBack(rapidjson::Value("PC:" + view->mId, doc.GetAllocator()), doc.GetAllocator());
    }
    for (const auto& viewCfg = config.fhirResourceViewConfiguration<Configuration::MedicationExporter>();
         const auto& view : viewCfg.allViews())
    {
        viewList.PushBack(rapidjson::Value("MX:" + view->mId, doc.GetAllocator()), doc.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer{buffer};
    doc.Accept(writer);
    respond(socket, std::string{buffer.GetString(), buffer.GetLength()});
}

void ErpFhirWS::validate(BoostSocket& socket, const std::string& query, const std::string& body)
{
    try {
        auto result = WsValidateHandler::handleRequest(socket.get_executor().context(), query, body);
        respond(socket, result.serializeToJsonString());

    }
    catch (const ErpFhirWsException& ex)
    {
        badRequest(socket, ex.what());
    }
    catch (const std::exception& ex)
    {
        TLOG(WARNING) << "Internal Server error handling request from " << socket.remote_endpoint() << ": " << ex.what();
        internalServerError(socket);
    }
}

void erp_fhir_ws::ErpFhirWS::servePublic(BoostSocket& socket, const std::string& path)
{
    std::filesystem::path fsPath{path == "/"?"/index.html":path};
    if (fsPath.parent_path().generic_string().find('.') != std::string::npos)
    {
        badRequest(socket, "invalid target");
        return;
    }
    static std::filesystem::path basePath{"erp-fhir-ws/public"};
    try {
        const auto& cwd = std::filesystem::current_path();
        auto resourcePath = (basePath / fsPath.relative_path());
        std::filesystem::path fileName = cwd.parent_path() / "shared" / resourcePath;
        if (!exists(fileName))
        {
            fileName = ResourceManager::getAbsoluteFilename(resourcePath.native());
        }
        auto content = FileHelper::readFileAsString(fileName);
        TVLOG(1) << "serving: " << fileName;

        if (fsPath.extension() == ".js")
        {
            respond(socket, content, "application/javascript");
        }
        else if (fsPath.extension() == ".html")
        {
            respond(socket, content, "text/html;charset=UTF-8");
        }
        else if (fsPath.extension() == ".css")
        {
            respond(socket, content, "text/css");
        }
        else if (fsPath.extension() == ".ico")
        {
            respond(socket, content, "image/x-icon");
        }
        else
        {
            respond(socket, content, "text/plain");
        }
    }
    catch (const std::runtime_error& ex)
    {
        respond(socket, "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>",
                "text/html", boost::beast::http::status::not_found);
    }
}

void ErpFhirWS::respond(BoostSocket& socket, std::string responseBody, std::string_view mimeType,
                                     boost::beast::http::status status)
{
    boost::system::error_code ec;
    boost::beast::http::response<boost::beast::http::string_body> response;
    response.body() = std::move(responseBody);
    response.set(boost::beast::http::field::server, "erp-fhir-ws/0.0.1");
    response.set(boost::beast::http::field::content_type, mimeType);
    response.keep_alive(false);
    response.result(status);
    response.prepare_payload();
    boost::beast::http::write(socket, response, ec);
    if (ec.failed())
    {
        TLOG(WARNING) << "error while writing response to "<< socket.remote_endpoint() << ": " << ec;
    }
}

void ErpFhirWS::badRequest(BoostSocket& socket, std::string_view message)
{
    TLOG(INFO) << "bad request from " << socket.remote_endpoint() <<  ": " << message;
    std::string body;
    body = "<html><head><title>400 Bad Request</title></head><body><h1>Bad Request</h1>";
    body += message;
    body += "</body></html>";
    respond(socket, std::move(body), "text/html", boost::beast::http::status::bad_request);
}

void ErpFhirWS::internalServerError(BoostSocket& socket)
{
    respond(socket,
            "<html><head><titel>500 Internal server error</title></head><body>"
            "<h1>500 Internal Server Error</h1>"
            "</body></html>",
            "text/html", boost::beast::http::status::bad_request);
}

}
