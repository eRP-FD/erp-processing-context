/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpProcessingContext.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/String.hxx"

#include "mock/crypto/MockCryptography.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/ServerTestBase.hxx"

#include <boost/format.hpp>

class SubscriptionHandlerTest : public ServerTestBase
{
public:
    ClientRequest makeEncryptedRequest(const HttpMethod method, const std::string_view endpoint,
                                       const std::string& body, const JWT& jwt1)
    {
        // Create the inner request
        auto header = Header(method, static_cast<std::string>(endpoint), Header::Version_1_1,
                             {{Header::ContentType, mMimeType},
                              {Header::Accept, mMimeType},
                              getAuthorizationHeaderForJwt(jwt1)},
                             HttpStatus::Unknown);
        ClientRequest request(header, body);

        // Encrypt with TEE protocol.
        ClientRequest encryptedRequest(
            Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
            teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt1));
        encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
        return encryptedRequest;
    }

    void testEndpoint(ClientRequest& request, HttpStatus expected, std::string& responseBody)
    {
        auto response = mClient.send(request);

        // Verify the outer response
        ASSERT_EQ(response.getHeader().status(), HttpStatus::OK)
            << "header status was " << toString(response.getHeader().status());
        if (response.getHeader().hasHeader(Header::ContentType))
        {
            ASSERT_EQ(response.getHeader().header(Header::ContentType), "application/octet-stream");
        }

        // Verify the inner response;
        const ClientResponse decryptedResponse = teeProtocol.parseResponse(response);

        responseBody = decryptedResponse.getBody();

        ASSERT_EQ(decryptedResponse.getHeader().status(), expected);
    }

    void testEndpoint(const HttpMethod method, const std::string_view endpoint, const std::string& body,
                      const JWT& jwt1, HttpStatus expected, std::string& responseBody)
    {

        // Send the request.
        auto req = makeEncryptedRequest(method, endpoint, body, jwt1);
        EXPECT_NO_FATAL_FAILURE(testEndpoint(req, expected, responseBody));
    }

    void testEndpoint(const HttpMethod method, const std::string_view endpoint, const std::string& body,
                      const JWT& jwt1, HttpStatus expected)
    {
        std::string responseBody;
        EXPECT_NO_FATAL_FAILURE(testEndpoint(method, endpoint, body, jwt1, expected, responseBody));
    }

    SubscriptionHandlerTest()
        : ServerTestBase(true)

        , jwtVersicherter(jwtWithProfessionOID(profession_oid::oid_versicherter))
        , jwtOeffentliche_apotheke(jwtWithProfessionOID(profession_oid::oid_oeffentliche_apotheke))
        , jwtKrankenhausapotheke(jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke))
        , teeProtocol()
        , mClient(createClient())
    {
    }


    JWT jwtVersicherter;
    JWT jwtOeffentliche_apotheke;
    JWT jwtKrankenhausapotheke;

    ClientTeeProtocol teeProtocol;
    HttpsClient mClient;
    ContentMimeType mMimeType{ContentMimeType::fhirJsonUtf8};
};


TEST_F(SubscriptionHandlerTest, PostSubscriptionRecipientParameter)
{
    const std::string_view endpoint = "/Subscription";

    // Test request with absent GET parameter "recipient" is not allowed.
    const auto* body0 =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body0, jwtOeffentliche_apotheke, HttpStatus::BadRequest));

    // Test request with capitalized GET parameter name is not allowed.
    const auto* body1 =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null&Recipient=123124125", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body1, jwtOeffentliche_apotheke, HttpStatus::BadRequest));
}

TEST_F(SubscriptionHandlerTest, PostSubscriptionStatusParameter)
{
    const std::string_view endpoint = "/Subscription";

    // Test request with status != requested is not allowed.
    const auto* body0 =
        R"--({ "resourceType": "Subscription", "status": "active", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient0", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body0, jwtOeffentliche_apotheke, HttpStatus::BadRequest));

    // Test request with empty Status is not allowed.
    const auto* body1 =
        R"--({ "resourceType": "Subscription", "status": "", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient0", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body1, jwtOeffentliche_apotheke, HttpStatus::BadRequest));

    // Test with absent status is not allowed.
    const auto* body2 =
        R"--({ "resourceType": "Subscription", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient0", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body2, jwtOeffentliche_apotheke, HttpStatus::BadRequest));
}

TEST_F(SubscriptionHandlerTest, PostSubscriptionTelematikIdMismatch)
{
    const std::string_view endpoint = "/Subscription";

    // Test request with recipient=recipient1 but the used jwt has recipient0.
    A_22363.test("Ensure that a LEI can register only for itself.");
    const auto* body0 =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient1", "channel": { "type": "websocket" } })--";
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body0, jwtOeffentliche_apotheke, HttpStatus::Forbidden));
    A_22363.finish();
}

TEST_F(SubscriptionHandlerTest, PostSubscriptionSuccessAuth)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard signingKey{"ERP_SERVICE_SUBSCRIPTION_SIGNING_KEY",
                                        std::string{MockCryptography::getEciesPrivateKeyPem()}};

    const std::string_view endpoint = "/Subscription";

    // Test valid request.
    const auto* body =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient0", "channel": { "type": "websocket" } })--";
    std::string responseBody;
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body, jwtOeffentliche_apotheke, HttpStatus::OK, responseBody));

    rapidjson::Document response;
    const rapidjson::ParseResult parseResult = response.Parse(responseBody);
    ASSERT_FALSE(parseResult.IsError());

    rapidjson::Pointer channelHeaderPointer("/channel/header/0");
    const auto* const headerField = channelHeaderPointer.Get(response);
    ASSERT_TRUE(headerField);
    std::string token = headerField->GetString();
    token = String::replaceAll(token, "Authorization: Bearer ", "");

    std::unique_ptr<JWT> jwt;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(token));

    ASSERT_NO_THROW(jwt->verifySignature(MockCryptography::getEciesPublicKey()));
}

TEST_F(SubscriptionHandlerTest, Overlap)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard signingKey{"ERP_SERVICE_SUBSCRIPTION_SIGNING_KEY",
                                        std::string{MockCryptography::getEciesPrivateKeyPem()}};

    const std::string_view endpoint = "/Subscription";

    // Test valid request.
    const auto* body =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=recipient0", "channel": { "type": "websocket" } })--";
    std::string responseBody;
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body, jwtOeffentliche_apotheke, HttpStatus::OK, responseBody));

    rapidjson::Document response;
    const rapidjson::ParseResult parseResult = response.Parse(responseBody);
    ASSERT_FALSE(parseResult.IsError());

    rapidjson::Pointer channelHeaderPointer("/channel/header/0");
    const auto* const headerField = channelHeaderPointer.Get(response);
    ASSERT_TRUE(headerField);
    std::string token = headerField->GetString();
    token = String::replaceAll(token, "Authorization: Bearer ", "");

    std::unique_ptr<JWT> jwt;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(token));

    ASSERT_NO_THROW(jwt->verifySignature(MockCryptography::getEciesPublicKey()));
}


TEST_F(SubscriptionHandlerTest, TelematicPseudonymChecks)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard signingKey{"ERP_SERVICE_SUBSCRIPTION_SIGNING_KEY",
                                        std::string{MockCryptography::getEciesPrivateKeyPem()}};
    const std::string_view endpoint = "/Subscription";
    // Test valid request.
    const std::string recipient = "recipient0";
    const std::string recipient1 = "recipient1";
    const std::string bodyFmt =
        R"--({ "resourceType": "Subscription", "status": "requested", "reason": "stay in touch", "criteria": "Communication?received=null&recipient=%1%", "channel": { "type": "websocket" } })--";
    const std::string body = boost::str(boost::format(bodyFmt.c_str()) % recipient);
    std::string responseBody;
    EXPECT_NO_FATAL_FAILURE(
        testEndpoint(HttpMethod::POST, endpoint, body, jwtOeffentliche_apotheke, HttpStatus::OK, responseBody));
    rapidjson::Document response;
    const rapidjson::ParseResult parseResult = response.Parse(responseBody);
    ASSERT_FALSE(parseResult.IsError());
    rapidjson::Pointer channelHeaderPointer("/channel/header/0");
    const auto* const headerField = channelHeaderPointer.Get(response);
    ASSERT_TRUE(headerField);
    std::string token = headerField->GetString();
    token = String::replaceAll(token, "Authorization: Bearer ", "");
    std::unique_ptr<JWT> jwt;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(token));
    ASSERT_NO_THROW(jwt->verifySignature(MockCryptography::getEciesPublicKey()));
    const rapidjson::Pointer idPointer("/id");
    const auto* const idField = idPointer.Get(response);
    ASSERT_TRUE(idField);
    std::string encryptedTelematicId = idField->GetString();
    // Check that the encrypted recipient (telematic) id is identical.
    EXPECT_EQ(mServer->serviceContext().getTelematicPseudonymManager().sign(recipient).hex(), encryptedTelematicId);
    // Check that different encrypted recipients do not match ((recipient0) != hash(recipient1))
    EXPECT_NE(mServer->serviceContext().getTelematicPseudonymManager().sign(recipient1).hex(), encryptedTelematicId);
}


TEST_F(SubscriptionHandlerTest, PostSubscriptionXML)
{
    const std::string_view endpoint = "/Subscription";

    // Test request with absent GET parameter "recipient" is not allowed.
    using namespace std::string_literals;
    const auto* body0 =
        R"--(
<Subscription xmlns="http://hl7.org/fhir">
    <status value="requested" />
    <reason value="none" />
    <criteria value="Communication?received=null&amp;recipient=recipient0" />
    <channel>
        <type value="websocket" />
    </channel>
</Subscription>
)--";
    std::string responseBody;
    mMimeType = ContentMimeType::fhirXmlUtf8;
    auto request = makeEncryptedRequest(HttpMethod::POST, endpoint, body0, jwtOeffentliche_apotheke);
    EXPECT_NO_FATAL_FAILURE(testEndpoint(request, HttpStatus::OK, responseBody));
    // Skip xml validation, just get an xml object and peek on single nodes.
    model::Subscription subscription = model::Subscription::fromXmlNoValidation(responseBody);
    EXPECT_EQ(subscription.status(), model::Subscription::Status::Active);
    EXPECT_EQ(subscription.reason(), "communication notifications");
}
