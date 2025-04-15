/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "erp/service/VauRequestHandler.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/JwtException.hxx"

#include "mock/crypto/MockCryptography.hxx"

#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test_config.h"

#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>


class VauRequestHandlerProblematicJwtsTest : public ServerTestBase, public ::testing::WithParamInterface<std::string>
{
public:
    shared_EVP_PKEY mPublicKey;
    shared_EVP_PKEY mPrivateKey;
    std::unique_ptr<JwtBuilder> mBuilder;

    void SetUp() override
    {
        ASSERT_NO_FATAL_FAILURE(ServerTestBase::SetUp());
        mPrivateKey = MockCryptography::getIdpPrivateKey();
        mPublicKey = MockCryptography::getIdpPublicKey();
        mBuilder = std::make_unique<JwtBuilder>(mPrivateKey);
    }

    std::string claimsFromTestFile(const std::string& path)
    {
        return FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + path);
    }

    ClientResponse sendRequest(JWT& jwt)
    {
        auto client = createClient();
        auto ep = GetParam();
        ClientRequest request(
            Header(HttpMethod::GET, std::move(ep), Header::Version_1_1, { getAuthorizationHeaderForJwt(jwt)}, HttpStatus::Unknown),
            "");
        ClientRequest encryptedRequest(
            Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
            teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt));
        encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
        // This header field will be present - we have to set it explicitly in the tests.
        encryptedRequest.setHeader(Header::ExternalInterface,
                                   std::string{Header::ExternalInterface_INTERNET});
        return client.send(encryptedRequest);
    }

    std::string jwtWithSignatureAlgorithm(const std::string& sigalg)
    {
        using namespace std::string_view_literals;
        const auto now = std::chrono::system_clock::now();
        const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + std::chrono::minutes(5));
        const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
        std::string payload
            = "{\"iat\":" + std::to_string(iat.count())
            + ",\"exp\":" + std::to_string(exp.count())
            + ",\"iss\":\"0\""
            + ",\"sub\":\"333\""
            + ",\"nonce\":\"0\""
            + ",\"acr\":\"" + std::string{JWT::acrContent} + "\""
            + ",\"aud\":\"https://gematik.erppre.de/\""
            + ",\"professionOID\":\"Test\""
            + ",\"given_name\":\"Vorname\""
            + ",\"family_name\":\"Nachname\""
            + ",\"organizationName\":\"Organisationsname\""
            + ",\"idNummer\":\"1212\""
            + ",\"jti\":\"0\""
            + "}";
        return std::string{Base64::encode("{\"alg\":\"" + sigalg + "\"}") + "." + Base64::encode(payload) + "." +
                           Base64::encode("invalid"sv)};
    };

    ClientTeeProtocol teeProtocol;
};

TEST_P(VauRequestHandlerProblematicJwtsTest, acrUnsupportedContent)//NOLINT(readability-function-cognitive-complexity)
{
    rapidjson::Document claims;
    ASSERT_NO_THROW(claims.Parse(claimsFromTestFile("/jwt/claims_patient.json")));

    const auto now = std::chrono::system_clock::now();

    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() +
                                                                      std::chrono::minutes(5));
    claims[std::string{JWT::expClaim}].SetInt64(exp.count());

    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claims[std::string{JWT::iatClaim}].SetInt64(iat.count());

    A_19439_02.test("Unit test for unsupported acr value.");
    claims[std::string{JWT::acrClaim}].SetString("Something");
    JWT jwt;
    ASSERT_NO_THROW(jwt = mBuilder->getJWT(claims));
    auto response = sendRequest(jwt);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    A_19439_02.finish();
}

TEST_P(VauRequestHandlerProblematicJwtsTest, acrValidContent)//NOLINT(readability-function-cognitive-complexity)
{
    rapidjson::Document claims;
    ASSERT_NO_THROW(claims.Parse(claimsFromTestFile("/jwt/claims_patient.json")));

    const auto now = std::chrono::system_clock::now();

    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() +
                                                                      std::chrono::minutes(5));
    claims[std::string{JWT::expClaim}].SetInt64(exp.count());

    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claims[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claims.RemoveMember(std::string{JWT::nbfClaim});

    A_19439_02.test("Unit test for supported acr value.");
    claims[std::string{JWT::acrClaim}].SetString(std::string{JWT::acrContent}.c_str(),
                                                 gsl::narrow_cast<rapidjson::SizeType>(JWT::acrContent.size()),
                                                 claims.GetAllocator());
    JWT jwt;
    ASSERT_NO_THROW(jwt = mBuilder->getJWT(claims));
    auto response = sendRequest(jwt);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    A_19439_02.finish();
}

TEST_P(VauRequestHandlerProblematicJwtsTest, UnsupportedSignatureAlgorithm)
{
    auto client = createClient();
    JWT invalidJwt{jwtWithSignatureAlgorithm("none") };
    auto ep = GetParam();
    ClientRequest request(Header(HttpMethod::GET, std::move(ep), Header::Version_1_1, { getAuthorizationHeaderForJwt(invalidJwt) }, HttpStatus::Unknown), "");
    A_19131.test("Handle unsupported signature algorithm - 'none' in this case.");
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwtWithSignatureAlgorithm("none")));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    encryptedRequest.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});
    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE( innerResponse.getHeader().hasHeader(Header::WWWAuthenticate) );
    ASSERT_TRUE( innerResponse.getHeader().header(Header::WWWAuthenticate).value() == std::string{VauRequestHandler::wwwAuthenticateErrorInvalidToken()} );
    A_19131.finish();
}

TEST_P(VauRequestHandlerProblematicJwtsTest, ValidSignatureAlgorithmWithInvalidSignature)
{
    auto client = createClient();
    JWT invalidJwt{jwtWithSignatureAlgorithm("BP256R1") };
    auto ep = GetParam();
    ClientRequest request(Header(HttpMethod::GET, std::move(ep), Header::Version_1_1, { getAuthorizationHeaderForJwt(invalidJwt) }, HttpStatus::Unknown), "");
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwtWithSignatureAlgorithm("BP256R1")));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    encryptedRequest.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});
    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    A_19131.test("Handle invalid signature content.");
    // The actual signature is still invalid (hard-coded string "invalid") even though we pass a supported signature algorithm.
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE( innerResponse.getHeader().hasHeader(Header::WWWAuthenticate) );
    ASSERT_TRUE( innerResponse.getHeader().header(Header::WWWAuthenticate).value() == std::string{VauRequestHandler::wwwAuthenticateErrorInvalidToken()} );
    A_19131.finish();
}

TEST_P(VauRequestHandlerProblematicJwtsTest, InvalidB64EncodedToken)
{
    A_19131.test("Handle invalid B64 encoding.");
    const std::string jwt = "HalloToken:eyJhbGciOiJCUDI1NlIxIiwidHlwIjoiSldUIiwia2lNIjoiUHJrX3Rva2VuIn0.eyJhY3IiOiJlaWRhcy1sb2EtaGlnaCIsImF1ZCI6Imh0dHBzOi8vZXJwLnRlbGVtYXRpay5kZS9sb2dpbiIsImV4cCI6MTYxNzExODI3NiwiZmFtaWx5X25hbWUiOiJkZXIgTmFjaG5hbWUiLCJnaXZlbl9uYW1lIjoiZGVyIFZvcm5hbWUiLCJpYXQiOjE2MTcxMTc5MTYsIm5iZiI6MTYxNzExNzk2NiwiaWROdW1tZXIiOiIxLUhCQS1UZXN0a2FydGUtODgzMTEwMDAwMTIwMzMwIiwiaXNzIjoiaHR0cHM6Ly9pZHAxLnRlbGVtYXRpay5kZS9qd3QiLCJqdGkiOiI2NmZhY2FjOS0xZTJiLTQ3NTAtOTAxNC0xM2M0YmY1Yjc3M2QiLCJub25jZSI6ImZ1dSBiYXIgYmF6Iiwib3JnYW5pemF0aW9uTmFtZSI6Ikluc3RpdHV0aW9ucy0gb2Rlci1PcmdhbmlzYXRpb25zLU5lemVpY2hudW5nIiwicHJvZmVzc2lvbk9JRCI6IjEuMi4yNzYuMC43Ni40LjMwIiwic3ViIjoiUmFiY1VTdXVXS0taRUVIbXJjTm1fa1VET1cxM3VhR1U1Wms4T29Cd2lOayJ9.XmokF3O5BZ7LtI4vftuCCVjz3Di9LH711oUinUrM4d8dBzw7IcfvTm4rNbWvNcjat0nzP8K3jatx0FekzuG_WA";

    auto client = createClient();
    auto ep = GetParam();
    ClientRequest request(Header(HttpMethod::GET, std::move(ep), Header::Version_1_1, { getAuthorizationHeaderForJwt(jwt) }, HttpStatus::Unknown), "");
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    encryptedRequest.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});

    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);

    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE( innerResponse.getHeader().hasHeader(Header::WWWAuthenticate) );
    ASSERT_TRUE( innerResponse.getHeader().header(Header::WWWAuthenticate).value() == std::string{VauRequestHandler::wwwAuthenticateErrorInvalidToken()} );
    A_19131.finish();
}

INSTANTIATE_TEST_SUITE_P(ForAllGETEndpoints, VauRequestHandlerProblematicJwtsTest,
                         testing::Values("/Task", "/metadata", "/Device", "/Communication", "/MedicationDispense",
                                         "/AuditEvent"));
