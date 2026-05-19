/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Bundle.hxx"
#include "test/exporter/mock/FhirVZDClientMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>
#include <openssl/ssl.h>
#include <chrono>


class FhirVZDClientTest : public testing::Test
{
public:
    void SetUp() override
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }

    // Override the settings from the configuration jsons. The values (at least urls, hosts and ports) must
    // match the settings in FhirVZDClientMock.
#define setupEnv() \
    EnvironmentVariableGuard v3("ERP_MEDICATION_EXPORTER_FHIR_VZD_CLIENT_TOKEN_URL", "localhost"); \
    EnvironmentVariableGuard v4("ERP_MEDICATION_EXPORTER_FHIR_VZD_CLIENT_TOKEN_PORT", "18443"); \
    EnvironmentVariableGuard v5("ERP_MEDICATION_EXPORTER_FHIR_VZD_CLIENT_API_URL", "localhost"); \
    EnvironmentVariableGuard v6("ERP_MEDICATION_EXPORTER_FHIR_VZD_CLIENT_API_PORT", "18443"); \
    EnvironmentVariableGuard v9("ERP_MEDICATION_EXPORTER_BFARM_CLIENT_URL", "localhost"); \
    EnvironmentVariableGuard v10("ERP_MEDICATION_EXPORTER_BFARM_CLIENT_PORT", "18443");

};


TEST_F(FhirVZDClientTest, happyPath)
{
    setupEnv();
    FhirVzdClientMock client("abc", "def");

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(300));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(600));
    FhirVzdClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 3);// Backend calls for access token, auth token and search.
}


TEST_F(FhirVZDClientTest, ExpiredAccessToken_Refresh_OK)
{
    setupEnv();
    FhirVzdClientMock client("abc", "def");

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    FhirVzdClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 3);

    FhirVzdClientMock::resetCallCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(2300));
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 1);// Backend call for search.
}


TEST_F(FhirVZDClientTest, ExpiredAccessAndAuthToken_Refresh_OK)
{
    setupEnv();
    FhirVzdClientMock client("abc", "def");

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    FhirVzdClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 3);

    FhirVzdClientMock::resetCallCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(4300));
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 3);
}


TEST_F(FhirVZDClientTest, InvalidAccessToken_Checks_OK)
{
    setupEnv();
    FhirVzdClientMock client("abc", "def");

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(0));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    FhirVzdClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle0;
    EXPECT_THROW(bundle0 = client.performSearch("123"), model::ModelException);
    EXPECT_EQ(bundle0.getResourceCount(), 0);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(), 1);


    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(0));

    model::Bundle bundle1;
    EXPECT_THROW(bundle1 = client.performSearch("123"), model::ModelException);
    EXPECT_EQ(bundle1.getResourceCount(), 0);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(),
              2);// Access token already in cache. 2 calls for auth and search request.

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(4));

    model::Bundle bundle2;
    EXPECT_THROW(bundle2 = client.performSearch({}), model::ModelException);
    EXPECT_EQ(bundle2.getResourceCount(), 0);
    EXPECT_EQ(FhirVzdClientMock::getCallCount(),
              0);// Access and auth token in cache. 3rd call failed (send() not yet called due to exception)
}


TEST_F(FhirVZDClientTest, CustomExceptions_Success)
{
    setupEnv();
    using namespace medication::exporter::exceptions;

    FhirVzdClientMock client("abc", "def");

    FhirVzdClientMock::resetCallCount();
    FhirVzdClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    FhirVzdClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    FhirVzdClientMock::setHttpStatus(HttpStatus::Unauthorized);

    model::Bundle bundle;
    EXPECT_THROW(bundle = client.performSearch("123"), AuthenticationException);

    FhirVzdClientMock::setHttpStatus(HttpStatus::Forbidden);

    EXPECT_THROW(
        {
            try
            {
                bundle = client.performSearch("123");
            }
            catch (const ServerErrorException& e)
            {
                EXPECT_EQ(e.statusCode(), HttpStatus::Forbidden);
                throw;
            }
        },
        ServerErrorException);
}
