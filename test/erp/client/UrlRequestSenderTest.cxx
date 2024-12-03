/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/common/Constants.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/server/RequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "shared/util/Expect.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <chrono>

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

    void handleRequest (BlockingTestSessionContext& session) override
    {
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
