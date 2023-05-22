/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/ErpMain.hxx"

#include "erp/client/HttpsClient.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/database/RedisClient.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/registration/RegistrationManager.hxx"
#include "erp/util/Condition.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/util/ThreadNames.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"

#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/mock/RegistrationMock.hxx"

#include <csignal>

/**
 *  The tests in this file take some time to finish.
 *
 *  All tests start the application, wait until it has initialized and is listening for incoming requests and then
 *  stop the application in different ways, e.g. by signal, or admin/shutdown request.
 */

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif


class ErpMainTest : public testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    void SetUp () override
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        MockTerminationHandler::setupForProduction();
    }

    void TearDown () override
    {
        MockTerminationHandler::setupForTesting();
        mCaDerPathGuard.reset();
    }

    /**
     * Start the application with all servers and asynchronous services.
     * Wait until they are all initialized and running.
     * Make some calls to the servers.
     * Terminate the application.
     */
    void runApplication (std::function<void(void)>&& terminationAction, Factories factories = StaticData::makeMockFactoriesWithServers())
    {
        ErpMain::StateCondition state (ErpMain::State::Unknown);
        auto processingContextThread = std::thread(
            [&state, &factories]
            {
                // Run the processing context in a thread so that it does not block the test.
                ThreadNames::instance().setCurrentThreadName("pc-main");
                if (!factories.tslManagerFactory)
                {
                    factories.tslManagerFactory = [](auto) {
                        const auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
                        const auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
                        const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
                        return TslTestHelper::createTslManager<TslManager>(
                            {}, {}, {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
                    };
                }
                ErpMain::runApplication(
                    std::move(factories),
                    state,
                    [](PcServiceContext& serviceContext)
                    {
                        // Set the certificate explicitly as a simple mock of the idp updater.
                        serviceContext.idp.setCertificate(Certificate(StaticData::idpCertificate));
                    });
            });

        // Initialization of the processing context takes some time. Wait until all is setup and the PC is waiting for
        // its termination.
        TVLOG(1) << "waiting for the processing context to finish its initialization";
        auto currentState = state.waitForValue(ErpMain::State::WaitingForTermination, std::chrono::seconds(60));
        ASSERT_EQ(currentState, ErpMain::State::WaitingForTermination);

        makeRequests();
        terminationAction();

        TVLOG(0) << "requested termination, waiting for that to finish";
        currentState = state.waitForValue(ErpMain::State::Terminated, std::chrono::seconds(15));
        ASSERT_EQ(currentState, ErpMain::State::Terminated);

        processingContextThread.join();
    }

    static bool usePostgres()
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }

    /**
     * Make one request each to an endpoint of the processing context and the enrolment service.
     * The goal is only to verify that the respective server sockets are active, not a functional test of the
     * called endpoints. Therefore the requests are as simple as possible and are not valid.
     * Make also one call to an unsupported port to verify that that leads to a different error condition.
     */
    void makeRequests ()
    {
        makeRequestToProcessingContext();
        makeRequestToEnrolmentService();

        makeRequestToUnknownPort();
    }


    void makeRequestToUnknownPort (void)
    {
        HttpsClient client ("127.0.0.1", 7090, 30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
        try
        {
            client.send(
                ClientRequest(
                    Header(HttpMethod::POST, "/VAU/0", 11, {}, HttpStatus::Unknown),
                    ""));
            // Expecting an exception
            FAIL();
        }
        catch(const boost::system::system_error& e)
        {
            ASSERT_EQ(e.code(), boost::asio::error::connection_refused);
        }
        catch(...)
        {
            FAIL();
        }
    }

    void makeRequestToProcessingContext (void)
    {
        const auto& config = Configuration::instance();
        HttpsClient client ("127.0.0.1", config.serverPort(), 30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
        const auto response = client.send(
            ClientRequest(
                Header(HttpMethod::POST, "/VAU/0", 11, {}, HttpStatus::Unknown),
                ""));

        ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
    }


    void makeRequestToEnrolmentService (void)
    {
        const auto& config = Configuration::instance();
        HttpsClient client("127.0.0.1",
                           gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ENROLMENT_SERVER_PORT)),
                           30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
        const auto response = client.send(
            ClientRequest(
                Header(HttpMethod::POST, "/", 11, {}, HttpStatus::Unknown),
                ""));

        ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
    }
};



TEST_F(ErpMainTest, runProcessingContext_manualTermination)
{
#ifdef NDEBUG
    GTEST_SKIP_("disabled in release build");
#endif
    runApplication(
        []{ TerminationHandler::instance().terminate();});
    SUCCEED();
}


TEST_F(ErpMainTest, runProcessingContext_SIGTERM)
{
#ifdef NDEBUG
    GTEST_SKIP_("disabled in release build");
#endif
    runApplication(
        []{(void)std::raise(SIGTERM);});
    SUCCEED();
}

TEST_F(ErpMainTest, runProcessingContext_adminShutdown)
{
    EnvironmentVariableGuard adminApiAuth{"ERP_ADMIN_CREDENTIALS", "cred"};
    runApplication(
        []
        {
            const auto& config = Configuration::instance();
        HttpsClient client(config.getStringValue(ConfigurationKey::ADMIN_SERVER_INTERFACE),
                           gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)), 30, false);
        const auto response = client.send(
                ClientRequest(
                    Header(HttpMethod::POST, "/admin/shutdown", 11,
                           {
                               {Header::ContentType, ContentMimeType::xWwwFormUrlEncoded},
                               {Header::Authorization, "Basic cred"}
                           },
                           HttpStatus::Unknown),
                    "delay-seconds=1"));

            ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
        });
    SUCCEED();
}

TEST_F(ErpMainTest, runProcessingContext_adminShutdownSIGTERM)
{
    EnvironmentVariableGuard adminApiAuth{"ERP_ADMIN_CREDENTIALS", "cred"};
    runApplication(
        []
        {
            const auto& config = Configuration::instance();
            HttpsClient client(config.getStringValue(ConfigurationKey::ADMIN_SERVER_INTERFACE),
                               gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)), 30, false);
            const auto response = client.send(
                ClientRequest(
                    Header(HttpMethod::POST, "/admin/shutdown", 11,
                           {
                               {Header::ContentType, ContentMimeType::xWwwFormUrlEncoded},
                               {Header::Authorization, "Basic cred"}
                           },
                           HttpStatus::Unknown),
                    "delay-seconds=1"));

            ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);

            (void)std::raise(SIGTERM);
        });
    SUCCEED();
}

TEST_F(ErpMainTest, runProcessingContext_initialTslDownloadFails)
{
    Factories factories = StaticData::makeMockFactoriesWithServers();
    factories.tslManagerFactory = [](auto) {
        const std::string tslContent =
            ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
        const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
        const auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
        const auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
        const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
        auto requestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{});
        requestSender->setUrlHandler(TslTestHelper::shaDownloadUrl, [](const std::string&) {
            return ClientResponse(Header(HttpStatus::InternalServerError), "");
        });

        return TslTestHelper::createTslManager<TslManager>(
            requestSender, {}, {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
    };

    auto processingContextThread = std::thread(
        [&factories]
        {
            ErpMain::StateCondition state (ErpMain::State::Unknown);
            // Run the processing context in a thread so that it does not block the test.
            ThreadNames::instance().setCurrentThreadName("pc-main");
            ASSERT_NO_FATAL_FAILURE(ErpMain::runApplication(
                std::move(factories),
                state,
                [](PcServiceContext& serviceContext)
                {
                    // Set the certificate explicitly as a simple mock of the idp updater.
                    serviceContext.idp.setCertificate(Certificate(StaticData::idpCertificate));
                    TerminationHandler::instance().terminate();
                }));
        });

    processingContextThread.join();
    SUCCEED();
}


TEST_F(ErpMainTest, getEnrolementServerPort_activeEnrolment)
{
    EnvironmentVariableGuard enrolmentPortGuard ("ERP_ENROLMENT_SERVER_PORT", "2345");
    EnvironmentVariableGuard activateForPortGuard ("ERP_ENROLMENT_ACTIVATE_FOR_PORT", "1234"); // matches the server port

    const auto enrolmentPort = getEnrolementServerPort(1234, 1002);

    // Expect the default was not used and that the enrolment server is active.
    ASSERT_TRUE(enrolmentPort.has_value());
    EXPECT_EQ(enrolmentPort.value(), 2345);
}


TEST_F(ErpMainTest, getEnrolementServerPort_inactiveEnrolment)
{
    EnvironmentVariableGuard enrolmentPortGuard ("ERP_ENROLMENT_SERVER_PORT", "2345");
    EnvironmentVariableGuard activateForPortGuard ("ERP_ENROLMENT_ACTIVATE_FOR_PORT", "1235"); // does not match the server port

    const auto enrolmentPort = getEnrolementServerPort(1234, 1002);

    // Expect that the default was not used and that the enrolment server is not active.
    ASSERT_FALSE(enrolmentPort.has_value());
}
