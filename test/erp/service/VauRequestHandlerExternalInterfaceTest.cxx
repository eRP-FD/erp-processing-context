/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/VauRequestHandler.hxx"

#include "erp/ErpProcessingContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/JwtException.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test_config.h"
#include "mock/crypto/MockCryptography.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>


class VauRequestHandlerExternalInterfaceTest : public ServerTestBase
{
public:
    std::string jwtWithWrongSignature()
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
            + ",\"aud\":\"0\""
            + ",\"professionOID\":\"Test\""
            + ",\"given_name\":\"Vorname\""
            + ",\"family_name\":\"Nachname\""
            + ",\"organizationName\":\"Organisationsname\""
            + ",\"idNummer\":\"1212\""
            + ",\"jti\":\"0\""
            + "}";
        return std::string{Base64::encode(R"({})"sv) + "." + Base64::encode(payload) + "." +
                           Base64::encode("invalid"sv)};
    }
};

TEST_F(VauRequestHandlerExternalInterfaceTest, InvalidJwtWithRequestINTERNET)
{
    auto client = createClient();
    JWT invalidJwt{ jwtWithWrongSignature() };
    ClientRequest request(Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(invalidJwt) }, HttpStatus::Unknown), "");
    request.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, invalidJwt));
    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE( innerResponse.getHeader().hasHeader(Header::WWWAuthenticate) );
    ASSERT_TRUE( innerResponse.getHeader().header(Header::WWWAuthenticate).value() == std::string{VauRequestHandler::wwwAuthenticateErrorInternetRequest()} );
}

TEST_F(VauRequestHandlerExternalInterfaceTest, InvalidJwtWithRequestTI)
{
    auto client = createClient();
    JWT invalidJwt{ jwtWithWrongSignature() };
    ClientRequest request(Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(invalidJwt) }, HttpStatus::Unknown), "");
    request.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_TI});
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, invalidJwt));
    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE( innerResponse.getHeader().hasHeader(Header::WWWAuthenticate) );
    ASSERT_TRUE( innerResponse.getHeader().header(Header::WWWAuthenticate).value() == std::string{VauRequestHandler::wwwAuthenticateErrorTiRequest() } );
}
