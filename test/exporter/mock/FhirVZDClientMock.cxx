#include "test/exporter/mock/FhirVZDClientMock.hxx"
#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/client/request/ClientRequest.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <boost/algorithm/string/replace.hpp>


namespace
{
const std::string TokenResponse{
    R"(
{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICJZYncycTFDajFHMFZxZ2wybzdYMG5OZFZUYWktNnZzTnpLUkJUbTNEcW9ZIn0.eyJleHAiOjE3NjMzOTQ5ODQsImlhdCI6MTc2MzM5NDY4NCwianRpIjoiOGJiYWY0YTMtYjg0MS00NWY3LThkNGYtYjFiZTQ2M2FhYWU0IiwiaXNzIjoiaHR0cHM6Ly9hdXRoLXJlZi52emQudGktZGllbnN0ZS5kZTo5NDQzL2F1dGgvcmVhbG1zL1NlcnZpY2UtQXV0aGVudGljYXRlIiwic3ViIjoiOGM3ZDZiYzktMTM1YS00ZTg5LWJjODYtMWFiNGEwYTNhMmI5IiwidHlwIjoiQmVhcmVyIiwiYXpwIjoiZ2VtaWJtZGVycGZkbW5vcHFyc3IiLCJzY29wZSI6IlNlcnZpY2UtQXV0aGVudGljYXRlIiwiY2xpZW50SG9zdCI6IjEwLjI0LjQ2LjExIiwiY2xpZW50QWRkcmVzcyI6IjEwLjI0LjQ2LjExIiwiY2xpZW50X2lkIjoiZ2VtaWJtZGVycGZkbW5vcHFyc3IifQ.ZPMfPK1RQAiIRlFlqsuN3LTLBDvlJ2ArwVJ79C83OFAoGi55BDvN3lj1b4Zo42eof6S0RjeDAGy9OaxtiLHPemoCmZEGerf_exs78alHVfqy13hBvn8XmBD5D5Hi-bpVhXF53uadjK2RjZlcr8BQSZ5ljnPrdJSGPqBOtjL1sNI__C-25kXf_3cuvb9hc3akumGbDMUTLKoMn9q248C9Ku07dkHGCGpKzEaKK39_3FeKvjvYmdMYRh-OebT9FZ3tVlXMct--pYo6PHSNoAuUSh5j5T9uQTP_UFr2Jn_gCZIiZ_CYLUw1cyVkMR90ChyyO1a8rd0bFmyyiHv7EeaVgA",
  "expires_in": ##EXPIRES_IN##,
  "refresh_expires_in": 0,
  "token_type": "Bearer",
  "not-before-policy": 0,
  "scope": "Service-Authenticate"
}
)"};

const std::string AuthResponse{
    R"(
{
  "access_token": "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJnZW1pYm1kZXJwZmRtbm9wcXJzciIsInNjb3BlIjoiY2VydGlmaWNhdGU6cmVhZCBmZHZfc2VhcmNoOnJlYWQgc2VhcmNoOnJlYWQiLCJpc3MiOiJodHRwczovL2ZoaXItZGlyZWN0b3J5LXJlZi52emQudGktZGllbnN0ZS5kZS9zZXJ2aWNlLWF1dGhlbnRpY2F0ZSIsImF1ZCI6WyJodHRwczovL2ZoaXItZGlyZWN0b3J5LXJlZi52emQudGktZGllbnN0ZS5kZS9mZHYvc2VhcmNoIiwiaHR0cHM6Ly9maGlyLWRpcmVjdG9yeS1yZWYudnpkLnRpLWRpZW5zdGUuZGUvc2VhcmNoIiwiaHR0cHM6Ly9maGlyLWRpcmVjdG9yeS1yZWYudnpkLnRpLWRpZW5zdGUuZGUvY2VydGlmaWNhdGVzIl0sImlhdCI6MTc2MzM5NDUxNCwiZXhwIjoxNzYzNDgwOTE0fQ.iBZmOp7yfk6wZy_ol0_M0ARorPhzhe-GLvZeI9IKfHRvKBswggziFPds2USQCJ0wrn2ZgwHViV4ynre0eEunxA",
  "client_id": "veryspecialid",
  "token_type": "Bearer",
  "expires_in": ##EXPIRES_IN##
}
)"};
}

std::chrono::seconds HttpsClientMock::mAccessTokenExpiration;
std::chrono::seconds HttpsClientMock::mAuthTokenExpiration;
size_t HttpsClientMock::mCallCounter;
HttpStatus HttpsClientMock::mHttpStatus;

HttpsClientMock::HttpsClientMock()
    : ClientInterface()
{
}


ClientResponse HttpsClientMock::send(const ClientRequest& clientRequest)
{
    mCallCounter++;
    if (clientRequest.getHeader().target() == "/auth/realms/Service-Authenticate/protocol/openid-connect/token")
    {
        auto str = TokenResponse;
        boost::replace_all(str, "##EXPIRES_IN##", std::to_string(mAccessTokenExpiration.count()));
        return ClientResponse{Header{mHttpStatus}, str};
    }
    else if (clientRequest.getHeader().target() == "/service-authenticate")
    {
        auto str = AuthResponse;
        boost::replace_all(str, "##EXPIRES_IN##", std::to_string(mAuthTokenExpiration.count()));
        return ClientResponse{Header{mHttpStatus}, str};
    }


    return ClientResponse{Header{mHttpStatus}, ResourceTemplates::vzdFhirBundleJson()};
}


FhirVzdClientMock::FhirVzdClientMock(std::string clientId)
    : FhirVzdClient(clientId, nullptr)
{
}


std::unique_ptr<ClientInterface> FhirVzdClientMock::createClient(const std::string& hostname [[maybe_unused]],
                                                                 const std::string& port [[maybe_unused]]) const
{
    return std::make_unique<HttpsClientMock>(HttpsClientMock{});
}
