/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Bundle.hxx"
#include "test/exporter/mock/FhirVZDClientMock.hxx"

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
};


TEST_F(FhirVZDClientTest, happyPath)
{

    FhirVzdClientMock client("abc");

    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(300));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(600));
    HttpsClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 3);// Backend calls for access token, auth token and search.
}


TEST_F(FhirVZDClientTest, ExpiredAccessToken_Refresh_OK)
{

    FhirVzdClientMock client("abc");

    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    HttpsClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 3);

    HttpsClientMock::resetCallCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(2300));
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 1);// Backend call for search.
}


TEST_F(FhirVZDClientTest, ExpiredAccessAndAuthToken_Refresh_OK)
{
    FhirVzdClientMock client("abc");

    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    HttpsClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 3);

    HttpsClientMock::resetCallCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(4300));
    EXPECT_NO_THROW(bundle = client.performSearch("123"));
    EXPECT_EQ(bundle.getResourceCount(), 3);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 3);
}


TEST_F(FhirVZDClientTest, InvalidAccessToken_Checks_OK)
{
    FhirVzdClientMock client("abc");

    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(0));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    HttpsClientMock::setHttpStatus(HttpStatus::OK);

    model::Bundle bundle;
    EXPECT_THROW(bundle = client.performSearch("123"), model::ModelException);
    EXPECT_EQ(bundle.getResourceCount(), 0);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 1);


    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(0));

    EXPECT_THROW(bundle = client.performSearch("123"), model::ModelException);
    EXPECT_EQ(bundle.getResourceCount(), 0);
    EXPECT_EQ(HttpsClientMock::getCallCount(), 2);// Access token already in cache. 2 calls for auth and search request.


    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(4));

    EXPECT_THROW(bundle = client.performSearch({}), model::ModelException);
    EXPECT_EQ(bundle.getResourceCount(), 0);
    EXPECT_EQ(HttpsClientMock::getCallCount(),
              0);// Access and auth token in cache. 3rd call failed (send() not yet called due to exception)
}


TEST_F(FhirVZDClientTest, CustomExceptions_Success)
{
    using namespace medication::exporter::exceptions;

    FhirVzdClientMock client("abc");

    HttpsClientMock::resetCallCount();
    HttpsClientMock::setAccessTokenExpiration(std::chrono::seconds(2));
    HttpsClientMock::setAuthTokenExpiration(std::chrono::seconds(4));
    HttpsClientMock::setHttpStatus(HttpStatus::Unauthorized);

    model::Bundle bundle;
    EXPECT_THROW(bundle = client.performSearch("123"), AuthenticationException);

    HttpsClientMock::setHttpStatus(HttpStatus::Forbidden);

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
