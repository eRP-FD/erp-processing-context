/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/UrlRequestSender.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "erp/server/HttpsServer.ixx"
#include "erp/server/ServerSocketHandler.ixx"
#include "erp/server/context/SessionContext.ixx"
#include "erp/server/handler/RequestHandlerContext.ixx"
#include "erp/server/handler/RequestHandlerManager.ixx"
#include "erp/server/session/ServerSession.ixx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Expect.hxx"

#include <gtest/gtest.h>
#include <chrono>

namespace
{
    constexpr const char* EXPECTED_BODY = "test request body";
    constexpr const char* HOST_IP = "127.0.0.1";
    constexpr const uint16_t PORT = 9988;
}


class BlockingTestServiceContext {};
using BlockingTestSessionContext = SessionContext<BlockingTestServiceContext>;

template class HttpsServer<BlockingTestServiceContext>;
template class RequestHandlerContainer<BlockingTestServiceContext>;
template class RequestHandlerContext<BlockingTestServiceContext>;
template class RequestHandlerManager<BlockingTestServiceContext>;
template class ServerSocketHandler<BlockingTestServiceContext>;
template class SessionContext<BlockingTestServiceContext>;

class BlockingTestServerHandler : public UnconstrainedRequestHandler<BlockingTestServiceContext>
{
public:
    BlockingTestServerHandler(std::function<bool()> isBlockedFunctor,
                              std::function<void(bool)> serverBlockingStatusSetter)
        : UnconstrainedRequestHandler<BlockingTestServiceContext>()
        , mIsBlockedFunctor(isBlockedFunctor)
        , mServerBlockingStatusSetter(serverBlockingStatusSetter)
    {
        Expect(isBlockedFunctor != nullptr, "the functor must be set");
    }

    virtual void handleRequest (BlockingTestSessionContext& session) override
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

    virtual Operation getOperation (void) const override
    {
        return Operation::UNKNOWN;
    }

private:
    std::function<bool()> mIsBlockedFunctor;
    std::function<void(bool)> mServerBlockingStatusSetter;
};


std::unique_ptr<HttpsServer<BlockingTestServiceContext>> createServer(
    const std::string& hostIp,
    uint16_t port,
    std::function<bool()> isBlockedFunctor,
    std::function<void(bool)> serverBlockingStatusSetter)
{
    RequestHandlerManager<BlockingTestServiceContext> handlerManager;
    std::unique_ptr<RequestHandlerInterface<BlockingTestServiceContext>> requestHandler =
         std::make_unique<BlockingTestServerHandler>(isBlockedFunctor, serverBlockingStatusSetter);
    handlerManager.onPostDo("/test_path", std::move(requestHandler));

    std::unique_ptr<BlockingTestServiceContext> serviceContext = std::make_unique<BlockingTestServiceContext>();

    return std::make_unique<HttpsServer<BlockingTestServiceContext>>(
        hostIp,
        port,
        std::move(handlerManager),
        std::move(serviceContext));
}


class UrlRequestSenderTest : public testing::Test
{
};


// TODO: create http-blocking-test for issue https://dth01.ibmgcloud.net/jira/browse/ERP-5935
TEST(UrlRequestSenderTest, testHttpsReadTimeout)
{
    std::atomic_bool blocking = true;
    std::atomic_bool serverBlockingStatus = false;
    std::unique_ptr<HttpsServer<BlockingTestServiceContext>> server = createServer(
        HOST_IP,
        PORT,
        [&blocking]() mutable -> bool
        { return blocking; },
        [&serverBlockingStatus](bool isBlocking) mutable -> void
        { serverBlockingStatus = isBlocking; });
    ASSERT_NE(server, nullptr) << "Server must be created";
    server->serve(1);

    UrlRequestSender urlRequestSender({}, 1, false);
    EXPECT_ANY_THROW(urlRequestSender.send("https://127.0.0.1:9988/test_path", HttpMethod::POST, EXPECTED_BODY));

    // the connection should be closed by timeout before the server decides to shutdown ( 10 seconds )
    // so the request handler still should run and block
    ASSERT_TRUE(serverBlockingStatus) << "the server blocked the request whole time";

    blocking = false;
    server->shutDown();
    server.reset();
}


TEST(UrlRequestSenderTest, ConnectionTimeoutHttps)  // NOLINT
{
    const uint16_t timeoutSeconds = 2;
    UrlRequestSender requestSender({}, timeoutSeconds);

    const auto start = std::chrono::steady_clock::now();
    // simulate unresponsive server by using a non-routable IP address
    EXPECT_ANY_THROW(requestSender.send("https://10.255.255.1/something", HttpMethod::GET, ""));
    const auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    EXPECT_LE(elapsedSeconds.count(), timeoutSeconds + 1.0);
}


TEST(UrlRequestSenderTest, ConnectionTimeoutHttp)  // NOLINT
{
    const uint16_t timeoutSeconds = 5;
    UrlRequestSender requestSender({}, timeoutSeconds);

    const auto start = std::chrono::steady_clock::now();
    // simulate unresponsive server by using a non-routable IP address
    EXPECT_ANY_THROW(requestSender.send("https://10.255.255.1/something", HttpMethod::GET, ""));
    const auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    EXPECT_LE(elapsedSeconds.count(), timeoutSeconds + 1.0);
}

