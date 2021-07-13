#include "erp/ErpProcessingContext.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/String.hxx"

#include "mock/crypto/MockCryptography.hxx"

#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ServerTestBase.hxx"

#include "tools/jwt/JwtBuilder.hxx"

#include <chrono>

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif

class VauRequestHandlerNotAllowedMethodTest : public ServerTestBase
{
public:
    ClientRequest makeEncryptedRequest (const HttpMethod method, const std::string_view endpoint, const JWT& jwt1)
    {
        // Create the inner request
        ClientRequest request(Header(
                method,
                static_cast<std::string>(endpoint),
                Header::Version_1_1,
                { getAuthorizationHeaderForJwt(jwt1) },
                HttpStatus::Unknown,
                false),
                "");

        // Encrypt with TEE protocol.
        ClientRequest encryptedRequest(Header(
                HttpMethod::POST,
                "/VAU/0",
                Header::Version_1_1,
                {},
                HttpStatus::Unknown,
                false),
                teeProtocol.createRequest(
                        MockCryptography::getEciesPublicKeyCertificate(),
                        request,
                        jwt1));
        encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
        return encryptedRequest;
    }

    void testEndpoint (const HttpMethod method, const std::string_view endpoint, const JWT& jwt1, HttpStatus expected)
    {
        auto response = mClient.send(makeEncryptedRequest(method, endpoint, jwt1));

        // Verify the outer response
        ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);

        ASSERT_FALSE(response.getBody().empty()) << "body is empty";

        // Verify the inner response;
        const ClientResponse decryptedResponse = teeProtocol.parseResponse(response);
        ASSERT_EQ(decryptedResponse.getHeader().status(), expected)
            << "header status was " << toString(response.getHeader().status());
    }

    VauRequestHandlerNotAllowedMethodTest()
        : jwtVersicherter(JwtBuilder::testBuilder().makeJwtVersicherter({"X987654321"})), teeProtocol(),
          mClient(createClient()),
          taskId(model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711)
                     .toString())
    {
    }

    std::unique_ptr<Database> createDatabaseFrontend()
    {
        auto databaseFrontend = ServerTestBase::createDatabase();
        dynamic_cast<MockDatabase&>(databaseFrontend->getBackend()).fillWithStaticData();
        return databaseFrontend;
    }


    JWT jwtVersicherter;

    ClientTeeProtocol teeProtocol;
    HttpsClient mClient;

    std::string taskId;
};

TEST_F(VauRequestHandlerNotAllowedMethodTest, NotAllowedMethodsTask)
{
    A_19030.test("Not allowed HTTP methods for /Task");
    std::string_view endpoint = "/Task/";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/$create";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>/$abort";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>/$accept";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>/$activate";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>/$close";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Task/<id>/$reject";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
}

TEST_F(VauRequestHandlerNotAllowedMethodTest, NotAllowedMethodsMedicationDispense)
{
    A_19400.test("Not allowed HTTP methods for /MedicationDispense");
    std::string_view endpoint = "/MedicationDispense/";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/MedicationDispense/<id>";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
}

TEST_F(VauRequestHandlerNotAllowedMethodTest, NotAllowedMethodsCommunication)
{
    A_19401.test("Not allowed HTTP methods for /Communication");
    std::string_view endpoint = "/Communication/";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/Communication/<id>";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
}


TEST_F(VauRequestHandlerNotAllowedMethodTest, NotAllowedMethodsAuditEvents)
{
    A_19402.test("Not allowed HTTP methods for /AuditEvent");
    std::string_view endpoint = "/AuditEvent/";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    endpoint = "/AuditEvent/<id>";
    testEndpoint(HttpMethod::HEAD, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
}
