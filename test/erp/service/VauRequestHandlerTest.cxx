#include "erp/ErpProcessingContext.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/client/HttpClient.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/JwtException.hxx"
#include "mock/crypto/MockCryptography.hxx"

#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ServerTestBase.hxx"

#include <gtest/gtest.h>
#include <test/util/EnvironmentVariableGuard.hxx>

#include "test_config.h"
#include "tools/jwt/JwtBuilder.hxx"


class VauRequestHandlerTest : public ServerTestBase
{
public:
    explicit VauRequestHandlerTest(bool forceMockDatabase = false)
        : ServerTestBase(forceMockDatabase) {}
    // T can be JWT or std::string, auto-resolved.
    template <typename T>
    ClientRequest makeEncryptedRequest(
        const HttpMethod method, const std::string_view endpoint, const T& jwt1,
        std::optional<std::string_view> xAccessCode = {},
        std::optional<std::pair<std::string_view, std::string_view>> bodyContentType = {},
        Header::keyValueMap_t&& headerFields = {},
        std::optional<std::function<void(std::string&)>> requestManipulator = {})  // used to simulate faulty messages
    {
        if (headerFields.find(Header::Authorization) == headerFields.end()) {
            headerFields.insert( getAuthorizationHeaderForJwt(jwt1) );
        }

        // Create the inner request
        ClientRequest request(Header(
            method,
            static_cast<std::string>(endpoint),
            Header::Version_1_1,
            std::move(headerFields),
            HttpStatus::Unknown,
            false),
            "");

        if (xAccessCode.has_value())
        {
            request.setHeader(Header::XAccessCode, std::string(*xAccessCode));
        }
        if (bodyContentType.has_value())
        {
            request.setBody(std::string(bodyContentType->first));
            request.setHeader(Header::ContentType, std::string(bodyContentType->second));
        }

        // Encrypt with TEE protocol.

        auto teeRequest = mTeeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt1);
        if(requestManipulator.has_value())
            (*requestManipulator)(teeRequest);

        ClientRequest encryptedRequest(Header(
            HttpMethod::POST,
            "/VAU/0",
            Header::Version_1_1,
            {},
            HttpStatus::Unknown,
            false),
            teeRequest);
        encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
        return encryptedRequest;
    }

    void SetUp (void) override
    {
        try
        {
            Environment::set("ERP_TOKEN_ULIMIT_CALLS", "5");
            startServer();
        }
        catch(...)
        {
            LOG(ERROR) << "starting the server failed";
            FAIL();
        }
    }


    void TearDown (void) override
    {
        Environment::unset("ERP_IDP_PUBLIC_KEY");
        Environment::unset("ERP_TOKEN_ULIMIT_CALLS");

        mServer->shutDown();
        mServer.reset();
    }

    ClientTeeProtocol mTeeProtocol; // Renamed to avoid compiler warning "C4458: Declaration of "teeProtocol" hides class members"
};

class VauRequestHandlerTestForceMockDB : public VauRequestHandlerTest
{
public:
    VauRequestHandlerTestForceMockDB()
        : VauRequestHandlerTest(true) {}
};

/**
 * This test focuses on the handling of the TEE protocol and not on the actual implementation of the endpoint.
 */
TEST_F(VauRequestHandlerTest, success)
{
    // Create a client
    auto client = createClient();
    Uuid uuid;
    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Communication/" + uuid.toString(), *mJwt);

    // Send the request.
    auto response = client.send(encryptedRequest);

    // Verify the outer response
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK)
                        << "header status was " << toString(response.getHeader().status());
    ASSERT_TRUE(response.getHeader().hasHeader(Header::ContentType));
    ASSERT_EQ(response.getHeader().header(Header::ContentType).value(), "application/octet-stream");

    // Verify the inner response;
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    ASSERT_EQ(decryptedResponse.getHeader().status(), HttpStatus::NotFound);
    ASSERT_FALSE(decryptedResponse.getHeader().hasHeader(Header::ContentType));
    ASSERT_TRUE(decryptedResponse.getHeader().hasHeader(Header::ContentLength));
    ASSERT_EQ(decryptedResponse.getHeader().contentLength(), 0);
}


TEST_F(VauRequestHandlerTest, failForMissingTeeWrapper)
{
    // Create a client
    auto client = createClient();

    // Create the inner request
    ClientRequest request(
        Header(
            HttpMethod::POST,
            "/Task/$create",
            Header::Version_1_1,
            {},
            HttpStatus::Unknown,
            false),
    "this is the request body");

    // Do not encrypt, and for this test more importantly, do not wrap the request, so that the original target /Task/$create is used as URL.

    auto response = client.send(request);

    // Verify the response
    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_F(VauRequestHandlerTest, failForExpiredJwt)
{
    using namespace std::chrono_literals;

    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 3s);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{MockCryptography::getIdpPrivateKey()};
    JWT localJwt = builder.getJWT(claim);

    // Create a client
    auto client = createClient();

    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Task", localJwt);

    // Send the request.
    auto response = client.send(encryptedRequest);

    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    A_19902.test("Unit test for JwtExpiredException");
    // Verify the outer response
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::WWWAuthenticate));
    ASSERT_EQ(innerResponse.getHeader().header(Header::WWWAuthenticate).value(), R"(Bearer realm='prescriptionserver.telematik', error='invalACCESS_TOKEN'")");
}

TEST_F(VauRequestHandlerTest, MissingRequiredClaim)
{
    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + std::chrono::minutes(5));
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());

    claim.RemoveMember(std::string{JWT::familyNameClaim});

    JwtBuilder builder{privateKey};
    JWT jwtTst;  // Changed from jwt to jwtTest to avoid compiler warning: Declaration of "jwt" hides class members
    ASSERT_NO_THROW(jwtTst = builder.getJWT(claim));

    auto client = createClient();

    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Task", jwtTst);

    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);

    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
}

TEST_F(VauRequestHandlerTest, ClaimTypeCheckFailure)
{
    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + std::chrono::minutes(5));
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    A_20370.test("Unit test for valid claims data types.");
    claim[std::string{JWT::familyNameClaim}] = 0xAA55;

    JwtBuilder builder{privateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    auto client = createClient();
    ClientRequest request(Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(jwt) },
                                 HttpStatus::Unknown, false), "");

    // Encrypt with TEE protocol.
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown, false),
        teeProtocol.createRequest(MockCryptography().getEciesPublicKeyCertificate(), request, jwt));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    // This header field will be present - we have to set it explicitly in the tests.
    encryptedRequest.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});

    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);

    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    A_20370.finish();
}

TEST_F(VauRequestHandlerTest, invalidSignatureJwt)
{
    using namespace std::chrono_literals;

    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + std::chrono::minutes(5));
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    JwtBuilder builder{privateKey};
    JWT localJwt;

    // Ensure that we have a perfectly valid jwt.
    ASSERT_NO_THROW(localJwt = builder.getJWT(claim));
    ASSERT_NO_THROW(localJwt.verify(publicKey));

    // Create a duplicate of the jwt above and manipulate its signature, thus rendering it invalid.
    std::string jwtStr = localJwt.serialize();
    auto parts = String::split(jwtStr, ".");
    parts[2][0] = '0'; // Corrupt the signature part.
    JWT manipulatedJwt(parts[0] + "." + parts[1] + "." + parts[2]);

    // Create a client
    auto client = createClient();
    // Create the inner request
    ClientRequest request(
        Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(manipulatedJwt) }, HttpStatus::Unknown, false),
        "");
    // Encrypt with TEE protocol.
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown, false),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, manipulatedJwt));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");

    // Send the request.
    auto response = client.send(encryptedRequest);

    A_19132.test("Unit test for JwtInvalidSignatureException");

    const ClientResponse innerResponse = teeProtocol.parseResponse(response);

    // At this point the jwt exception is already handled and we can only evaluate the http status code ...
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    // Just make sure that it was really an invalid signature exception and not something else.
    ASSERT_THROW(manipulatedJwt.verify(MockCryptography::getEciesPublicKey()), JwtInvalidSignatureException);

    A_19132.finish();
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskAbort)
{
    A_20703.test("BruteForceHeaderTaskAbort");

    dynamic_cast<MockDatabase&>(*mMockDatabase).fillWithStaticData();
    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.716.71/$abort?secret=wrong_secret",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }

    {
        // patient
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.717.68/$abort",
            jwtWithProfessionOID(profession_oid::oid_versicherter), "wrong_ac");
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskAbort)
{
    A_20703.test("NoBruteForceHeaderTaskAbort");

    mMockDatabase->fillWithStaticData();
    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST,
            "/Task/160.000.000.004.716.71/"
            "$abort?secret=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }

    {
        // patient
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.717.68/$abort",
            jwtWithProfessionOID(profession_oid::oid_versicherter),
            "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskAccept)
{
    A_20703.test("BruteForceHeaderTaskAccept");

    mMockDatabase->fillWithStaticData();
    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.714.77/$accept?ac=wrong_ac",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}
TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskAccept)
{
    A_20703.test("NoBruteForceHeaderTaskAccept");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.714.77/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskActicate)
{
    A_20703.test("BruteForceHeaderTaskActicate");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // arzt
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.713.80/$activate",
            jwtWithProfessionOID(profession_oid::oid_arzt), "wrong_ac");
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}
TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskActicate)
{
    A_20703.test("NoBruteForceHeaderTaskActicate");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // arzt
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.713.80/$activate",
            jwtWithProfessionOID(profession_oid::oid_arzt), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskClose)
{
    A_20703.test("BruteForceHeaderTaskClose");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.716.71/$close?secret=wrong_secret",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}
TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskClose)
{
    A_20703.test("NoBruteForceHeaderTaskClose");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.716.71/$close?secret=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskReject)
{
    A_20703.test("BruteForceHeaderTaskReject");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.716.71/$reject?secret=wrong_secret",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}
TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskReject)
{
    A_20703.test("NoBruteForceHeaderTaskReject");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST, "/Task/160.000.000.004.716.71/$reject?secret=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, BruteForceHeaderTaskGet)
{
    A_20703.test("BruteForceHeaderTaskGet");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::GET, "/Task/160.000.000.004.716.71?secret=wrong_secret",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }

    {
        // patient
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::GET, "/Task/160.000.000.004.716.71",
            jwtWithProfessionOID(profession_oid::oid_versicherter), "wrong_ac");
        auto response = client.send(encryptedRequest);

        ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
        ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
                  "brute_force");
    }
}

TEST_F(VauRequestHandlerTestForceMockDB, NoBruteForceHeaderTaskGet)
{
    A_20703.test("NoBruteForceHeaderTaskGet");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::GET, "/Task/160.000.000.004.716.71?secret=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }

    {
        // patient
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::GET, "/Task/160.000.000.004.716.71",
            jwtWithProfessionOID(profession_oid::oid_versicherter), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
        auto response = client.send(encryptedRequest);

        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    }
}


TEST_F(VauRequestHandlerTestForceMockDB, InvalidPrescriptionHeaderTaskActivate)
{
    A_20704.start("InvalidPrescriptionHeaderTaskActivate");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    std::string parameters = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value="INVALIDPKCS7"/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/160.000.000.004.713.80/$activate",
                             jwtWithProfessionOID(profession_oid::oid_arzt),
                             "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
                             std::make_pair(parameters, "application/fhir+xml"));
    auto response = client.send(encryptedRequest);

    ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
    ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode),
              "invalid_prescription");
}

TEST_F(VauRequestHandlerTestForceMockDB, NoInvalidPrescriptionHeaderTaskActivate)
{
    A_20704.start("NoInvalidPrescriptionHeaderTaskActivate");

    mMockDatabase->fillWithStaticData();

    auto client = createClient();

    const auto cadesBesSignatureFile = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/kbv_bundle.xml.p7s");
    std::string parameters = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +  cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/160.000.000.004.713.80/$activate",
                             jwtWithProfessionOID(profession_oid::oid_arzt),
                             "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
                             std::make_pair(parameters, "application/fhir+xml"));
    auto response = client.send(encryptedRequest);

    ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
}

TEST_F(VauRequestHandlerTest, UnsupportedAcceptHeaderERP4620)
{
    const auto* body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create",
            jwtWithProfessionOID(profession_oid::oid_arzt),{},
            std::make_pair(body, "application/fhir+xml"), {{Header::Accept, "application/jpeg"}});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::NotAcceptable);
}

TEST_F(VauRequestHandlerTest, FormatRequestParameterFhirJson)
{
    const auto* body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=fhir%2Bjson",
                             jwtWithProfessionOID(profession_oid::oid_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::Created);
    EXPECT_EQ(decryptedResponse.getHeader().header("Content-Type"), "application/fhir+json;charset=utf-8");
}

TEST_F(VauRequestHandlerTest, FormatRequestParameterFhirXml)
{
    const auto* body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=fhir%2Bxml",
                             jwtWithProfessionOID(profession_oid::oid_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::Created);
    EXPECT_EQ(decryptedResponse.getHeader().header("Content-Type"), "application/fhir+xml;charset=utf-8");
}

TEST_F(VauRequestHandlerTest, InvalidFormatRequestParameter)
{
    const auto* body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=jpeg",
                             jwtWithProfessionOID(profession_oid::oid_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::NotAcceptable);
}

TEST_F(VauRequestHandlerTest, InvalidAcceptHeaderERP4620)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    const std::string header =
        "POST /Task/$create HTTP/1.1\r\nContent-Type: application/json\r\nAccept:ungültig\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n";
    auto client = createClient();

    ClientRequest encryptedRequest(Header(
        HttpMethod::POST,
        "/VAU/0",
        Header::Version_1_1,
        {},
        HttpStatus::Unknown,
        false),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            header + body,
            jwtWithProfessionOID(profession_oid::oid_arzt)));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");

    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerTest, EmptyAcceptHeaderERP4620)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    const std::string header =
        "POST /Task/$create HTTP/1.1\r\nContent-Type: application/json\r\nAccept:\r\nContent-Length: "
            + std::to_string(body.size()) + "\r\n\r\n";
    auto client = createClient();

    ClientRequest encryptedRequest(Header(
        HttpMethod::POST,
        "/VAU/0",
        Header::Version_1_1,
        {},
        HttpStatus::Unknown,
        false),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            header + body,
            jwtWithProfessionOID(profession_oid::oid_arzt)));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");

    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerTest, failForMissingTls)
{
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create",
            jwtWithProfessionOID(profession_oid::oid_arzt),{},
            std::make_pair("", "application/fhir+xml"));
    auto client = HttpClient("127.0.0.1", 9999, 30/*connectionTimeoutSeconds*/);

    // As the server does not understand HTTP without TLS it can not send a proper error response.
    // But we can detect that the response is not sent completely (if at all) by looking for an exception.
    ASSERT_THROW(
        client.send(encryptedRequest),
        std::runtime_error);
}

TEST_F(VauRequestHandlerTest, VerifyTokenBlocklisting)
{
    EnvironmentVariableGuard enableDosCheck{"DEBUG_DISABLE_DOS_CHECK", "false"};
    auto client = createClient();
    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/AuditEvent/", *mJwt);
    // ERP_TOKEN_ULIMIT_CALLS is set to 5 in the test setup and it is expected that the 6th request
    // triggers the blocklisting and return the expected status (429 - too many requests)
    auto response = client.send(encryptedRequest);
    for (int i=0; i<4; i++)
    {
        response = client.send(encryptedRequest);
        // Verify the outer response
        ASSERT_EQ(response.getHeader().status(), HttpStatus::OK) << "header status was " << toString(response.getHeader().status());
        // Verify the inner response;
        const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
        ASSERT_EQ(decryptedResponse.getHeader().status(), HttpStatus::OK);
    }
    // 6th request triggers the expected error
    response = client.send(encryptedRequest);

    // Verify the outer response
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK) << "header status was " << toString(response.getHeader().status());
    ASSERT_TRUE(response.getHeader().hasHeader(Header::VAUErrorCode));
    ASSERT_EQ(response.getHeader().header(Header::VAUErrorCode), "brute_force");
    // Verify the inner response;
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    ASSERT_EQ(decryptedResponse.getHeader().status(), HttpStatus::TooManyRequests);
}

TEST_F(VauRequestHandlerTest, RequestWithJwtConstructorException)
{
    using namespace std::chrono_literals;

    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5min);
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() - 1min);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{MockCryptography::getIdpPrivateKey()};
    JWT localJwt = builder.getJWT(claim);
    std::string jwtStr = localJwt.serialize();
    jwtStr += ".";

    // Create a client
    auto client = createClient();

    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Task", jwtStr);

    // Send the request.
    auto response = client.send(encryptedRequest);

    // There will be no valid inner session, check the outer response.
    EXPECT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
}

TEST_F(VauRequestHandlerTest, failMessageToShort)
{
    // Create a client
    auto client = createClient();
    auto encryptedRequest = makeEncryptedRequest(
        HttpMethod::GET, "/Communication/" + Uuid().toString(), *mJwt, {}, {}, {},
        [](std::string& teeRequest) { teeRequest = teeRequest.substr(0, 60); });

    // Send the request.
    auto response = client.send(encryptedRequest);

    // Verify the response status
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest)
        << "header status was " << toString(response.getHeader().status());
}

TEST_F(VauRequestHandlerTest, failMessageInvalidVersion)
{
    // Create a client
    auto client = createClient();
    auto encryptedRequest = makeEncryptedRequest(
        HttpMethod::GET, "/Communication/" + Uuid().toString(), *mJwt, {}, {}, {},
        [](std::string& teeRequest) { teeRequest[0] = 111; });

    // Send the request.
    auto response = client.send(encryptedRequest);

    // Verify the response status
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest)
        << "header status was " << toString(response.getHeader().status());
}

TEST_F(VauRequestHandlerTest, failMessageInvalidPublicKey)
{
    // Create a client
    auto client = createClient();
    auto encryptedRequest = makeEncryptedRequest(
        HttpMethod::GET, "/Communication/" + Uuid().toString(), *mJwt, {}, {}, {},
        [](std::string& teeRequest) { teeRequest[1] = 42; });

    // Send the request.
    auto response = client.send(encryptedRequest);

    // Verify the response status
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest)
        << "header status was " << toString(response.getHeader().status());
}
