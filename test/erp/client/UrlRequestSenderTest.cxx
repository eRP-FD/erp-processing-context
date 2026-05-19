/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/UrlRequestSender.hxx"
#include "mock/client/TlsCertificateVerifierNoVerificationImplementation.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "shared/common/Constants.hxx"
#include "shared/server/RequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/Expect.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/HttpServer.hxx"

#include <boost/asio/ssl/stream.hpp>
#include <chrono>
#include <fmt/format.h>
#include <gtest/gtest.h>

namespace
{
    constexpr const char* EXPECTED_BODY = "test request body";
    constexpr const char* HOST_IP = "127.0.0.1";
}


using BlockingTestSessionContext = SessionContext;

class BlockingTestServerHandler : public UnconstrainedRequestHandler
{
public:
    BlockingTestServerHandler(std::function<bool()> isBlockedFunctor,
                              std::function<void(bool)> serverBlockingStatusSetter)
        : UnconstrainedRequestHandler()
        , mIsBlockedFunctor(std::move(isBlockedFunctor))
        , mServerBlockingStatusSetter(std::move(serverBlockingStatusSetter))
    {
        Expect(mIsBlockedFunctor != nullptr, "the isBlockedFunctor must be set");
    }

    void handleRequest (BaseSessionContext& baseSession) override
    {
        auto& session = dynamic_cast<BlockingTestSessionContext&>(baseSession);
        if (session.request.getBody() == EXPECTED_BODY)
        {
            mServerBlockingStatusSetter(true);
            // avoid blocking longer than 10 seconds, that should be more than enough for the test
            int counter = 0;
            while(mIsBlockedFunctor() && ++counter < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            mServerBlockingStatusSetter(false);
        }

        session.response.setStatus(HttpStatus::OK);
        session.response.setBody("");
    }

    Operation getOperation (void) const override
    {
        return Operation::UNKNOWN;
    }



private:
    std::function<bool()> mIsBlockedFunctor;
    std::function<void(bool)> mServerBlockingStatusSetter;
};


class UrlRequestSenderTest : public testing::Test
{
public:
    std::unique_ptr<HttpsServer> createServer(
        const std::string& hostIp,
        uint16_t port,
        std::function<bool()> isBlockedFunctor,
        std::function<void(bool)> serverBlockingStatusSetter)
    {
        RequestHandlerManager handlerManager;
        std::unique_ptr<RequestHandlerInterface> requestHandler =
                std::make_unique<BlockingTestServerHandler>(std::move(isBlockedFunctor),
                                                            std::move(serverBlockingStatusSetter));
        handlerManager.onPostDo("/test_path", std::move(requestHandler));

        return std::make_unique<HttpsServer>(
            hostIp,
            port,
            std::move(handlerManager),
            serviceContext);
    }

    PcServiceContext& getServiceContext()
    {
        return serviceContext;
    }
private:
    PcServiceContext serviceContext = StaticData::makePcServiceContext();
};


// TODO: create http-blocking-test for issue https://dth01.ibmgcloud.net/jira/browse/ERP-5935
TEST_F(UrlRequestSenderTest, testHttpsReadTimeout)//NOLINT(readability-function-cognitive-complexity)
{
    std::atomic_bool blocking = true;
    std::atomic_bool serverBlockingStatus = false;
    const auto& config = Configuration::instance();
    const auto port = config.serverPort() + 10;
    std::unique_ptr<HttpsServer> server = createServer(
        HOST_IP,
        gsl::narrow<uint16_t>(port),
        [&blocking]() mutable -> bool
        { return blocking; },
        [&serverBlockingStatus](bool isBlocking) mutable -> void
        { serverBlockingStatus = isBlocking; });
    ASSERT_NE(server, nullptr) << "Server must be created";
    server->serve(1, "test");

    UrlRequestSender urlRequestSender(Configuration::instance().getStringValue(ConfigurationKey::SERVER_CERTIFICATE), 1,
                                      Constants::resolveTimeout);
    std::stringstream url;
    url << "https://127.0.0.1:" << port << "/test_path";
    EXPECT_ANY_THROW(urlRequestSender.send(url.str(), HttpMethod::POST, EXPECTED_BODY));

    // the connection should be closed by timeout before the server decides to shutdown ( 10 seconds )
    // so the request handler still should run and block
    ASSERT_TRUE(serverBlockingStatus) << "the server blocked the request whole time";

    blocking = false;
    server->shutDown();
    server.reset();
}


TEST_F(UrlRequestSenderTest, ConnectionTimeoutHttps)  // NOLINT
{
    const uint16_t timeoutSeconds = 2;
    UrlRequestSender requestSender({}, timeoutSeconds, Constants::resolveTimeout);

    const auto start = std::chrono::steady_clock::now();
    // simulate unresponsive server by using a non-routable IP address
    EXPECT_ANY_THROW(requestSender.send("https://10.255.255.1/something", HttpMethod::GET, ""));
    const auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    EXPECT_LE(elapsedSeconds.count(), timeoutSeconds + 1.0);
}


TEST_F(UrlRequestSenderTest, ConnectionTimeoutHttp)  // NOLINT
{
    const uint16_t timeoutSeconds = 5;
    UrlRequestSender requestSender({}, timeoutSeconds, Constants::resolveTimeout);

    const auto start = std::chrono::steady_clock::now();
    // simulate unresponsive server by using a non-routable IP address
    EXPECT_ANY_THROW(requestSender.send("https://10.255.255.1/something", HttpMethod::GET, ""));
    const auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    EXPECT_LE(elapsedSeconds.count(), timeoutSeconds + 1.0);
}


TEST_F(UrlRequestSenderTest, proxyUrlIsUsedAndOriginalHostInHeader)
{
    const auto& config = Configuration::instance();
    const auto proxyPort = gsl::narrow<uint16_t>(config.serverPort() + 11);


    std::atomic_bool triedFirstProxy{false};
    auto closingRequestHandler = [&](boost::asio::ip::tcp::socket& s, const HttpServer::RequestType&) -> HttpServer::ResponseType {
        triedFirstProxy = true;
        s.close();
        return {};
    };
    auto closingProxyServer = HttpServer(boost::asio::ip::make_address("127.0.0.1"), proxyPort+1, closingRequestHandler);
    auto closingProxyServerThread = std::thread([&closingProxyServer] {
        closingProxyServer.run();
    });

    std::atomic_bool requestReceived{false};
    std::string receivedHost;
    std::string receivedBody;
    std::string receivedPath;
    auto requestHandler = [&](boost::asio::ip::tcp::socket&, const HttpServer::RequestType& request) -> HttpServer::ResponseType {
        requestReceived = true;
        receivedHost = request["Host"];
        receivedBody = request.body();
        receivedPath = request.target();

        HttpServer::ResponseType response{};
        response.result(boost::beast::http::status::ok);
        response.keep_alive(false);
        return response;
    };

    auto proxyServer = HttpServer(boost::asio::ip::make_address("127.0.0.1"), proxyPort, requestHandler);
    auto proxyServerThread = std::thread([&proxyServer] {
        proxyServer.run();
    });

    UrlRequestSender urlRequestSender(
        TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
        std::chrono::milliseconds(50), Constants::resolveTimeout);
    // add three configurations
    // - the first one will not work, which disconnected immediately
    // - the second does not have a server (connection refused)
    // - third works
    auto closingProxyConfig = ProxyParameters::fromUrl(fmt::format("https://127.0.0.1:{}", proxyPort+1), ProxyMode::HTTP);
    auto deadProxyConfig = ProxyParameters::fromUrl(fmt::format("https://127.0.0.1:{}", proxyPort+2), ProxyMode::HTTP);
    auto proxyConfig = ProxyParameters::fromUrl(fmt::format("https://127.0.0.1:{}", proxyPort), ProxyMode::HTTP);
    urlRequestSender.setProxies({closingProxyConfig, deadProxyConfig, proxyConfig});

    const uint16_t fakeTargetPort = 19999;
    const std::string targetUrl = fmt::format("http://localhost:{}/test_path", fakeTargetPort);

    ASSERT_NO_THROW(auto response = urlRequestSender.send(targetUrl, HttpMethod::POST, EXPECTED_BODY, targetUrl));

    EXPECT_TRUE(triedFirstProxy) << "Request was not sent to closingProxyServer";

    ASSERT_TRUE(requestReceived) << "Request must be forwarded to proxy server";
    const std::string expectedHost = fmt::format("localhost:{}", fakeTargetPort);
    EXPECT_EQ(receivedHost, expectedHost) << "Host header must contain original target host, not proxy host";
    EXPECT_EQ(receivedBody, EXPECTED_BODY);
    EXPECT_EQ(receivedPath, "/test_path");


    // ensure the connection fails if we have only dead proxies
    urlRequestSender.setProxies({closingProxyConfig, deadProxyConfig});
    EXPECT_ANY_THROW(urlRequestSender.send(targetUrl, HttpMethod::POST, EXPECTED_BODY, targetUrl));

    closingProxyServer.stop();
    closingProxyServerThread.join();
    proxyServer.stop();
    proxyServerThread.join();
}


class ProxyVerifyingHandler : public UnconstrainedRequestHandler
{
public:
    std::atomic_bool requestReceived{false};
    std::string receivedHost;
    std::string receivedBody;
    std::string receivedPath;

    void handleRequest(BaseSessionContext& baseSession) override
    {
        auto& session = dynamic_cast<BlockingTestSessionContext&>(baseSession);
        const auto& header = session.request.header();

        receivedHost = header.header(Header::Host).value_or("????????");
        receivedBody = session.request.getBody();
        receivedPath = std::string(header.target());
        requestReceived = true;

        session.response.setStatus(HttpStatus::OK);
        session.response.setBody("");
    }

    Operation getOperation(void) const override
    {
        return Operation::UNKNOWN;
    }
};


TEST_F(UrlRequestSenderTest, proxySniIsUsedAndOriginalHostInHeader)
{
    const auto& config = Configuration::instance();
    const auto proxyPort = gsl::narrow<uint16_t>(config.serverPort() + 11);
    auto handlerOwner = std::make_unique<ProxyVerifyingHandler>();
    const ProxyVerifyingHandler* handlerPtr = handlerOwner.get();

    RequestHandlerManager handlerManager;
    handlerManager.onPostDo("/test_path", std::move(handlerOwner));
    auto proxyServer = std::make_unique<HttpsServer>(HOST_IP, gsl::narrow<uint16_t>(proxyPort),
                                                     std::move(handlerManager), getServiceContext());
    proxyServer->serve(1, "proxyServer");
    ASSERT_NE(proxyServer, nullptr);
    UrlRequestSender urlRequestSender(TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
                                      std::chrono::milliseconds(50), Constants::resolveTimeout);
    auto proxyConfig = ProxyParameters::fromUrl(fmt::format("https://127.0.0.1:{}", proxyPort), ProxyMode::SNI);
    urlRequestSender.setProxies({proxyConfig});

    const uint16_t fakeTargetPort = 19999;
    const std::string targetUrl = fmt::format("https://localhost:{}/test_path", fakeTargetPort);

    ASSERT_NO_THROW(auto response = urlRequestSender.send(targetUrl, HttpMethod::POST, EXPECTED_BODY, targetUrl));

    ASSERT_TRUE(handlerPtr->requestReceived) << "Request must be forwarded to proxy server";

    const std::string expectedHost = fmt::format("localhost:{}", fakeTargetPort);
    EXPECT_EQ(handlerPtr->receivedHost, expectedHost)
        << "Host header must contain original target host, not proxy host";
    EXPECT_EQ(handlerPtr->receivedBody, EXPECTED_BODY);
    EXPECT_EQ(handlerPtr->receivedPath, "/test_path");
    proxyServer->shutDown();
}
