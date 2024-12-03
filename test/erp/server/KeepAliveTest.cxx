/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "test/util/ServerTestBase.hxx"

#include "shared/service/Operation.hxx"


namespace {
    class ErrorHandler : public RequestHandlerInterface
    {
    public:
        void handleRequest (BaseSessionContext&) override
        {
            throw std::runtime_error("simulated error in request handler implementation");
        }
        [[nodiscard]] bool allowedForProfessionOID(std::string_view) const override
        {
            return true;
        }
        Operation getOperation (void) const override
        {
            return Operation::GET_Device;
        }
    };
}


class KeepAliveTest : public ServerTestBase
{
public:
    constexpr static size_t manyRequestCount = 1000;

    KeepAliveTest (void)
        : mDosCheckGuard("DEBUG_DISABLE_DOS_CHECK", "TRUE")
    {
    }

    void addAdditionalPrimaryHandlers (RequestHandlerManager& manager) override
    {
        manager.addRequestHandler(
                HttpMethod::GET,
                "/error",
                std::make_unique<ErrorHandler>());
    }

    /**
     * Return a VAU encrypted simple request that does not require input arguments or setup of any database.
     */
    ClientRequest createSimpleRequest (void)
    {
        return encryptRequest(
            ClientRequest(
                Header(
                    HttpMethod::GET,
                    "/Device",
                    Header::Version_1_1,
                    {
                        {Header::Accept, ContentMimeType::fhirJsonUtf8},
                        {Header::Authorization, "Bearer " + mJwt->serialize()}
                    },
                    HttpStatus::Unknown),
                ""));
    }
    ClientRequest createSimpleRequestWithoutEncryption (std::string path = "/Device")
    {
        return ClientRequest(
            Header(
                HttpMethod::GET,
                std::move(path),
                Header::Version_1_1,
                {
                    {Header::Accept, ContentMimeType::fhirJsonUtf8},
                    {Header::Authorization, "Bearer " + mJwt->serialize()}
                },
                HttpStatus::Unknown),
            "");
    }

private:
    EnvironmentVariableGuard mDosCheckGuard;
};


TEST_F(KeepAliveTest, oneRequest_noKeepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "FALSE");

    auto client = createClient();
    {
        // The first request is expected to be successful.
        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    }
}


TEST_F(KeepAliveTest, DISABLED_twoRequests_noKeepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "FALSE");

    auto client = createClient();
    {
        // The first request is expected to be successful.
        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    }

    // Recent changes let the server return in its outer header "Connection: close" which is now
    // handled by the client by closing the connection and setting up a new one for the next request.
    // Until we find a way to simulate the old behavior of not adding the "Connection: close" header,
    // this test will no longer fail. => disabled

    {
        // The second, identical request is expected to fail because it is made via the same connection and would
        // require the support of keep-alive.
        ASSERT_ANY_THROW(
            client.send(createSimpleRequest()));
    }
}


TEST_F(KeepAliveTest, twoRequests_keepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "TRUE");

    auto client = createClient();
    {
        // The first request is expected to be successful.
        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    }
    {
        // The second, identical request is expected mot to fail because we have activate support for keeping the connection alive.
        ASSERT_NO_THROW(
            client.send(createSimpleRequest()));
    }
}


TEST_F(KeepAliveTest, clientClosesConnection_keepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "TRUE");

    // Make enough requests so that one thread services at least two requests.
    for (size_t index=0; index<serverThreadCount+1; ++index)
    {
        auto client = createClient();
        // The first request is expected to be successful.
        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    }
}


TEST_F(KeepAliveTest, manyRequests_noKeepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "FALSE");

    std::chrono::steady_clock::duration totalDuration;
    std::array<size_t,manyRequestCount> durations{};

    for (size_t index=0; index<manyRequestCount; ++index)
    {
        const auto start = std::chrono::steady_clock::now();

        auto client = createClient();
        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

        const auto end = std::chrono::steady_clock::now();

        totalDuration += end-start;
        durations[index] = gsl::narrow<size_t>(std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

        TVLOG(1) << "request on client side took " << static_cast<double>(durations[index])/1.e3 << "ms to complete";
    }

    std::sort(durations.begin(), durations.end());
    size_t totalMicroSeconds = 0;
    for (size_t index=10; index<manyRequestCount-10; ++index)
        totalMicroSeconds += durations[index];

    TVLOG(0) << "\n\n" << (manyRequestCount-20) << " requests took a total of "
            << gsl::narrow<double>(totalMicroSeconds)/1e6
            << " s, which is " << gsl::narrow<double>(totalMicroSeconds/1000)/1e3
            << "ms per request";
    TVLOG(0) << "smallest value is " << durations[0] << ", greatest value is " << durations[manyRequestCount-1];
}


TEST_F(KeepAliveTest, manyRequests_keepAlive)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "TRUE");

    if (!Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_DOS_CHECK, false))
    {
        GTEST_SKIP_("DOS check could not be disabled");
    }

    auto client = createClient();
    std::chrono::steady_clock::duration totalDuration;
    std::array<size_t,manyRequestCount> durations{};

    for (size_t index=0; index<manyRequestCount; ++index)
    {
        const auto start = std::chrono::steady_clock::now();

        const auto outerResponse = client.send(createSimpleRequest());
        const auto innerResponse = verifyOuterResponse(outerResponse);
        ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

        const auto end = std::chrono::steady_clock::now();

        totalDuration += end-start;
        durations[index] = gsl::narrow<size_t>(std::chrono::duration_cast<std::chrono::microseconds>(end-start).count());

        TVLOG(1) << "request on client side took " << gsl::narrow<double>(durations[index])/1.e3 << "ms to complete";
    }

    std::sort(durations.begin(), durations.end());
    size_t totalMicroSeconds = 0;
    for (size_t index=10; index<manyRequestCount-10; ++index)
        totalMicroSeconds += durations[index];

    TVLOG(0) << "\n\n" << (manyRequestCount-20) << " requests took a total of "
            << gsl::narrow<double>(totalMicroSeconds)/1e6
            << " s, which is " << gsl::narrow<double>(totalMicroSeconds/1000)/1e3
            << "ms per request";
    TVLOG(0) << "smallest value is " << durations[0] << ", greatest value is " << durations[manyRequestCount-1];
}


/**
 * Verify that errors that are detected while reading the request are caught and reported back to the caller.
 */
TEST_F(KeepAliveTest, oneRequest_failForBadRequest)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "TRUE");

    // The first request is expected to be successful.
    auto request = createSimpleRequest();
    // Add a large header entry that puts the total size of the header over the limit of 8kB.
    // This will trigger an error in ServerRequestReader that is detected before the request body or other
    // parts of the header are examined.
    request.setHeader("large-item", std::string(8192 + 1, 'X'));

    const auto response = createClient().send(request);

    // Expect a bad request response.
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
}


TEST_F(KeepAliveTest, oneRequest_failForBadRequestHandler)
{
    EnvironmentVariableGuard isKeepAliveSupported ("ERP_SERVER_KEEP_ALIVE", "TRUE");

    // The first request is expected to be successful.
    auto request = createSimpleRequestWithoutEncryption("/error");
    const auto response = createClient().send(request);

    // Expect a bad request response.
    ASSERT_EQ(response.getHeader().status(), HttpStatus::InternalServerError);
}
