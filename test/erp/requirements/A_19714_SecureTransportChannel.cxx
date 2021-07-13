#include "test/util/ServerTestBase.hxx"

#include "erp/crypto/EllipticCurve.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "tools/jwt/JwtBuilder.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include "test_config.h"

/**
 * The tests in this file illustrate the assessment in ERP_1404 (https://dth01.ibmgcloud.net/jira/browse/ERP-1404).
 *
 * They show that a request that fails for a given request because of invalid input values
 * fails with different error messages when either TLS or the VAU protocol is NOT used.
 * As evidence that tests for valid TLS and VAU protocol are executed before the inner request is analysed and
 * the later checks are not reached.
 *
 * 1. The base test will make a call to GET /Communication/<id> with an invalid id value.
 * 2. The same bad request will then be made with a JWT that is valid in itself but not based on the VAU certificate.
 * 3. Finally the same bad request is repeated without VAU encryption. This time the inner VAURequestHandler that
 *    previously failed to verify the JWT is not even reached. The outer request handler only knows about POST /VAU/<UP>.
 */

class A_19714_Test : public ServerTestBase
{
    virtual void SetUp() override
    {
        tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
        ServerTestBase::SetUp();
    }

    virtual void TearDown() override
    {
        tslEnvironmentGuard.reset();
        ServerTestBase::TearDown();
    }

    std::unique_ptr<EnvironmentVariableGuard> tslEnvironmentGuard;
};


/**
 * Call GET /Communication/<id> with an id for which there is NO object in the database.
 */
TEST_F(A_19714_Test, getCommunicationById_failForMissingObject)
{
    // Create the inner request
    ClientRequest innerRequest(
        createGetHeader("/Communication/xyz"),
        //                               ^ invalid id format, id should be a uuid value => Bad Request
        "");

    // Send the request. Note that the client uses a jwt that has `userA` as caller.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(innerRequest));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    ASSERT_FALSE(innerResponse.getHeader().header(Header::ContentType).has_value());
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentLength));
    ASSERT_EQ(innerResponse.getHeader().contentLength(), 0);
}


TEST_F(A_19714_Test, getCommunicationById_failForInvalidJWT)
{
    // Keep the database empty.

    // Create the inner request
    ClientRequest innerRequest(
        createGetHeader("/Communication/xyz"),
        //                               ^ invalid id format, id should be a uuid value
        "");

    // Send the request. Note that the client uses a jwt that has `userA` as caller.
    auto client = createClient();
    // Instead of just calling encryptRequest(request) we duplicate that method so that we can inject an invalid JWT.
    // => Unauthorized
    JwtBuilder builder (EllipticCurve::BrainpoolP256R1->createKeyPair());
    auto invalidJwt = std::make_unique<JWT>( builder.makeJwtVersicherter("X234567890") );
    innerRequest.setHeader(Header::Authorization, "Bearer " + invalidJwt->serialize());
    ClientRequest outerRequest(
        Header(
            HttpMethod::POST,
            "/VAU/0",
            Header::Version_1_1,
            {},
            HttpStatus::Unknown,
            false),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            innerRequest,
            *invalidJwt));
    outerRequest.setHeader(Header::ContentType, "application/octet-stream");
    auto outerResponse = client.send(outerRequest);
    auto innerResponse = mTeeProtocol.parseResponse(outerResponse);
    // The invalid JWT is expected to trigger a 401 Unauthorized in the inner request, outer request is still OK.
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
}


TEST_F(A_19714_Test, getCommunicationById_failForMissingVAUProtocol)
{
    // Keep the database empty.

    // Create the inner request
    ClientRequest innerRequest(
        createGetHeader("/Communication/xyz"),
        //                               ^ invalid id format, id should be a uuid value
        "");

    // Send the request. Note that the client uses a jwt that has `userA` as caller.
    auto client = createClient();
    // Instead of calling encryptRequest(request) we send the inner request without encrypting it.
    // As the JWT is contained in the preamble of the encrypted body, we don't have a use for a JWT, valid or otherwise.
    auto outerResponse = client.send(innerRequest);

    // The missing encryption leads to 404 Not Found not because the communication object can not be found but
    // because the GET /Communication endpoint is not available directly to outside callers.
    // Only POST /VAU/<user-pseudonym> is exposed.
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::NotFound);
}
