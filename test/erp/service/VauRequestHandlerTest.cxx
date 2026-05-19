/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpProcessingContext.hxx"
#include "erp/crypto/DtbpPseudonymization.hxx"
#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/common/Constants.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/network/client/HttpClient.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/JwtException.hxx"
#include "test_config.h"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/ServerTestBase.hxx"

#include <gtest/gtest.h>
#include <test/util/EnvironmentVariableGuard.hxx>
#include <chrono>
#include <codecvt>

template<class Facet>
struct UsableFacet : public Facet {
    using Facet::Facet;
    ~UsableFacet() override = default;
};
template<typename internT, typename externT, typename stateT>
using codecvt = UsableFacet<std::codecvt<internT, externT, stateT>>;

class VauRequestHandlerTest : public ServerTestBase
{
public:
    explicit VauRequestHandlerTest(bool forceMockDatabase = false)
        : ServerTestBase(forceMockDatabase) {}

    static constexpr int timespan_ms = 4000;
    EnvironmentVariableGuard calls{"ERP_TOKEN_ULIMIT_CALLS", "5"};
    EnvironmentVariableGuard timespan{"ERP_TOKEN_ULIMIT_TIMESPAN_MS", std::to_string(timespan_ms)};
    EnvironmentVariableGuard featureToggleGuard{"ERP_FEATURE_EU", "true"};
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
TEST_F(VauRequestHandlerTest, success)//NOLINT(readability-function-cognitive-complexity)
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
    ASSERT_TRUE(decryptedResponse.getHeader().hasHeader(Header::ContentType)); // error message (not found)
    ASSERT_TRUE(decryptedResponse.getHeader().hasHeader(Header::ContentLength));
    ASSERT_GT(decryptedResponse.getHeader().contentLength(), 0);
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
            HttpStatus::Unknown),
    "this is the request body");

    // Do not encrypt, and for this test more importantly, do not wrap the request, so that the original target /Task/$create is used as URL.

    auto response = client.send(request);

    // Verify the response
    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_F(VauRequestHandlerTest, failForExpiredJwt)//NOLINT(readability-function-cognitive-complexity)
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
    // Verify the outer response
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::WWWAuthenticate));
    ASSERT_EQ(innerResponse.getHeader().header(Header::WWWAuthenticate).value(), R"(Bearer realm='prescriptionserver.telematik', error='invalACCESS_TOKEN')");
}

TEST_F(VauRequestHandlerTest, MissingRequiredClaim)//NOLINT(readability-function-cognitive-complexity)
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

    claim.RemoveMember(std::string{JWT::displayNameClaim});

    JwtBuilder builder{privateKey};
    JWT jwtTst;  // Changed from jwt to jwtTest to avoid compiler warning: Declaration of "jwt" hides class members
    ASSERT_NO_THROW(jwtTst = builder.getJWT(claim));

    auto client = createClient();

    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Task", jwtTst);

    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);

    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
}

TEST_F(VauRequestHandlerTest, ClaimTypeCheckFailure)//NOLINT(readability-function-cognitive-complexity)
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
    claim[std::string{JWT::displayNameClaim}] = 0xAA55;

    JwtBuilder builder{privateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    auto client = createClient();
    ClientRequest request(Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(jwt) },
                                 HttpStatus::Unknown), "");

    // Encrypt with TEE protocol.
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
        teeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    // This header field will be present - we have to set it explicitly in the tests.
    encryptedRequest.setHeader(Header::ExternalInterface, std::string{Header::ExternalInterface_INTERNET});

    auto response = client.send(encryptedRequest);
    const ClientResponse innerResponse = teeProtocol.parseResponse(response);

    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);
    A_20370.finish();
}

TEST_F(VauRequestHandlerTest, invalidSignatureJwt)//NOLINT(readability-function-cognitive-complexity)
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
        Header(HttpMethod::GET, "/Task", Header::Version_1_1, { getAuthorizationHeaderForJwt(manipulatedJwt) }, HttpStatus::Unknown),
        "");
    // Encrypt with TEE protocol.
    ClientTeeProtocol teeProtocol;
    ClientRequest encryptedRequest(
        Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
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

TEST_F(VauRequestHandlerTest, idpAllowUseSecondaryCert)//NOLINT(readability-function-cognitive-complexity)
{
    const auto originalIdpCert = mContext->idp.getCertificate();
    mContext->idp.setCertificate(Certificate::createSelfSignedCertificateMock(MockCryptography::getEciesPrivateKey()));

    auto client = createClient();
    Uuid uuid;
    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Communication/" + uuid.toString(), *mJwt);
    // Send the request.
    auto response = client.send(encryptedRequest);
    // Verify the outer response
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto innerResponse = mTeeProtocol.parseResponse(response);
    // Verify the inner response is not authorized
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::Unauthorized);

    // set secondary certificate to the right idp cert
    mContext->idp.setSecondaryCertificate(Certificate(originalIdpCert));
    encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/Communication/" + uuid.toString(), *mJwt);
    // Send the request.
    response = client.send(encryptedRequest);
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    innerResponse = mTeeProtocol.parseResponse(response);
    // Verify the inner response
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);

    mContext->idp.resetSecondaryCertificate();
    mContext->idp.setCertificate(Certificate(originalIdpCert));
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
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt), "wrong_ac");
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
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt), "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea");
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
                             jwtWithProfessionOID(profession_oid::oid_praxis_arzt),
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

    const auto cadesBesSignatureFile =
        CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml());
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
                             jwtWithProfessionOID(profession_oid::oid_praxis_arzt),
                             "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
                             std::make_pair(parameters, "application/fhir+xml"));
    auto response = client.send(encryptedRequest);

    ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
}

TEST_F(VauRequestHandlerTest, UnsupportedAcceptHeaderERP4620)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--(/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create",
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt),{},
            std::make_pair(body, "application/fhir+xml"), {{Header::Accept, "application/jpeg"}});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::NotAcceptable);
}

TEST_F(VauRequestHandlerTest, FormatRequestParameterFhirJson)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--("/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=fhir%2Bjson",
                             jwtWithProfessionOID(profession_oid::oid_praxis_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::Created);
    EXPECT_EQ(decryptedResponse.getHeader().header("Content-Type"), "application/fhir+json;charset=utf-8");
}

TEST_F(VauRequestHandlerTest, FormatRequestParameterFhirXml)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--("/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=fhir%2Bxml",
                             jwtWithProfessionOID(profession_oid::oid_praxis_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::Created);
    EXPECT_EQ(decryptedResponse.getHeader().header("Content-Type"), "application/fhir+xml;charset=utf-8");
}

TEST_F(VauRequestHandlerTest, InvalidFormatRequestParameter)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--("/><code value="160"/>
    </valueCoding></parameter></Parameters>)--";
    auto client = createClient();
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create?_format=jpeg",
                             jwtWithProfessionOID(profession_oid::oid_praxis_arzt),{},
                             std::make_pair(body, "application/fhir+xml"), {});
    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::NotAcceptable);
}

TEST_F(VauRequestHandlerTest, InvalidAcceptHeaderERP4620)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--("/><code value="160"/>
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
        HttpStatus::Unknown),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            header + body,
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt)));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");

    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerTest, EmptyAcceptHeaderERP4620)
{
    const std::string body =
        R"--(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/>
    <valueCoding><system value=")--" +
        std::string(model::resource::code_system::flowType) + R"--("/><code value="160"/>
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
        HttpStatus::Unknown),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            header + body,
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt)));
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");

    auto response = client.send(encryptedRequest);
    const ClientResponse decryptedResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(decryptedResponse.getHeader().status(), HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerTest, failForMissingTls)
{
    auto encryptedRequest =
        makeEncryptedRequest(HttpMethod::POST, "/Task/$create",
            jwtWithProfessionOID(profession_oid::oid_praxis_arzt),{},
            std::make_pair("", "application/fhir+xml"));
    const auto& config = Configuration::instance();
    auto client =
        HttpClient("127.0.0.1", gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)),
                   std::chrono::seconds{30} /*connectionTimeout*/, Constants::resolveTimeout);

    // As the server does not understand HTTP without TLS it can not send a proper error response.
    // But we can detect that the response is not sent completely (if at all) by looking for an exception.
    ASSERT_THROW(
        client.send(encryptedRequest),
        std::runtime_error);
}

TEST_F(VauRequestHandlerTest, VerifyTokenBlocklisting)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard enableDosCheck{"DEBUG_DISABLE_DOS_CHECK", "false"};

    auto client = createClient();
    auto encryptedRequest = makeEncryptedRequest(HttpMethod::GET, "/AuditEvent/", *mJwt);

    {
        using namespace std::chrono;
        // This code for bucket computation is extracted from DosHandler.cxx - if it wouldn't trigger
        // security assessments, this piece of code should be extracted and reused. For now, I tend
        // to copy/paste it here.
        auto tNow = time_point_cast<milliseconds>(system_clock::now());
        auto nowVal = tNow.time_since_epoch().count();

        auto tSpan = time_point<system_clock, milliseconds>( milliseconds( timespan_ms ) );
        auto spanVal = tSpan.time_since_epoch().count();
        auto tBucket = nowVal - (nowVal % spanVal);

        // If remaining bucket time is less than 2000 ms, wait until next bucket time. This should
        // give the test enough (bucket) time after the sleep call.
        auto jitter = timespan_ms - (nowVal - tBucket);
        if (jitter < timespan_ms/2)
        {
            std::this_thread::sleep_for(milliseconds(jitter));
        }
    }

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
    WITH_RETRIES()
    {
        // Create a client
        auto client = createClient();
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::GET, "/Communication/" + Uuid().toString(), *mJwt, {}, {}, {},
            [](std::string& teeRequest) { ++teeRequest[1]; });

        // Send the request.
        auto response = client.send(encryptedRequest);

        // Verify the response status
        SOFT_EXPECT_TRUE(response.getHeader().status() == HttpStatus::BadRequest);
        if (HasFailure())
        {
            TVLOG(0) << "header status was " << toString(response.getHeader().status());
        }
    }
}

TEST_F(VauRequestHandlerTest, CheckClientId)//NOLINT(readability-function-cognitive-complexity)
{
    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    auto client = createClient();
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    JwtBuilder builder{privateKey};

    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto exp = duration_cast<seconds>(now.time_since_epoch() + minutes(5));
    const auto iat = duration_cast<seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());

    // Test 1 - No client id in token, outer response does not provide InnerRequestClientId.
    JWT token0;
    ASSERT_NO_THROW(token0 = builder.getJWT(claim));
    auto response0 = client.send(makeEncryptedRequest(HttpMethod::GET, "/Task", token0));
    const ClientResponse innerResponse0 = mTeeProtocol.parseResponse(response0);
    // Check that the response succeeded.
    ASSERT_EQ(innerResponse0.getHeader().status(), HttpStatus::OK);
    ASSERT_FALSE(response0.getHeader().hasHeader(Header::InnerRequestClientId));

    // Test 2 - Empty client id in token, outer response does not provide InnerRequestClientId.
    JWT token1;
    claim.AddMember(rapidjson::Value{std::string{JWT::clientIdClaim}, claim.GetAllocator()}, rapidjson::Value{"", claim.GetAllocator()}, claim.GetAllocator());
    ASSERT_NO_THROW(token1 = builder.getJWT(claim));
    auto response1 = client.send(makeEncryptedRequest(HttpMethod::GET, "/Task", token0));
    const ClientResponse innerResponse1 = mTeeProtocol.parseResponse(response1);
    // Check that the response succeeded.
    ASSERT_EQ(innerResponse1.getHeader().status(), HttpStatus::OK);
    ASSERT_FALSE(response0.getHeader().hasHeader(Header::InnerRequestClientId));

    // Test 3 - Provide client id in token, expect InnerRequestClientId in outer response.
    claim[std::string{JWT::clientIdClaim}].SetString(JwtBuilder::defaultClientId(), claim.GetAllocator());
    JWT token2;
    ASSERT_NO_THROW(token2 = builder.getJWT(claim));
    auto response2 = client.send(makeEncryptedRequest(HttpMethod::GET, "/Task", token2));
    const ClientResponse innerResponse2 = mTeeProtocol.parseResponse(response2);
    // Check that the response succeeded.
    ASSERT_EQ(innerResponse2.getHeader().status(), HttpStatus::OK);
    // Now check that the header field is present in the outer response with the expected value.
    ASSERT_TRUE(response2.getHeader().hasHeader(Header::InnerRequestClientId));
    ASSERT_EQ(response2.getHeader().header(Header::InnerRequestClientId).value(), JwtBuilder::defaultClientId());
}


TEST_F(VauRequestHandlerTest, NoRateLimit_oid_ncpeh)//NOLINT(readability-function-cognitive-complexity)
{
    auto client = createClient();
    const auto xmlStr = ResourceTemplates::euCloseTaskXml({});
    const model::PrescriptionId taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714);
    const auto jwt = std::make_unique<JWT>( jwtWithProfessionOID(profession_oid::oid_ncpeh) );
    const auto key = std::string{mContext->getDosHandler().redisKeyPrefix()} + ":" + jwt->stringForClaim(JWT::subClaim).value();
    auto encryptedRequest = makeEncryptedRequest(HttpMethod::POST, "/Task/" + taskId.toString() + "/$eu-close/", *jwt,
        {}, std::make_pair<std::string_view, std::string_view>(xmlStr, "application/fhir+xml;charset=utf-8"), {{Header::ContentType, ContentMimeType::fhirXmlUtf8}});
    client.send(encryptedRequest);
    EXPECT_FALSE(mContext->getRedisClient()->exists(key));
}


TEST_F(VauRequestHandlerTest, pnLogKeyPatient)//NOLINT(readability-function-cognitive-complexity)
{
    auto pseudonymizationKey = mHsmPool->acquire().session().getPseudonameLogKey();
    ASSERT_TRUE(pseudonymizationKey.has_value());
    const auto key = SafeString(std::move(*pseudonymizationKey));
    const std::string claimString =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    JWT token;
    ASSERT_NO_THROW(token = mJwtBuilder.getJWT(claim));
    auto client = createClient();
    auto req = makeEncryptedRequest(HttpMethod::GET, "/Task", token);
    req.setHeader(Header::XForwardedFor, "127.0.0.1");
    auto response = client.send(req);
    EXPECT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    // VauRequestHandler does not set the header field because
    // GET /Task is performed with oid == versicherter:
    EXPECT_FALSE(response.getHeader().hasHeader(Header::PnTelematikId));
    ASSERT_TRUE(response.getHeader().hasHeader(Header::PnIpaddress));

    const auto ipAddress =
        DtbpPseudonymization::decryptLogData(response.getHeader().header(Header::PnIpaddress).value(), key);
    EXPECT_EQ(ipAddress, "127.0.0.1");
}


TEST_F(VauRequestHandlerTest, pnLogKeyProfession)//NOLINT(readability-function-cognitive-complexity)
{
    auto pseudonymizationKey = mHsmPool->acquire().session().getPseudonameLogKey();
    ASSERT_TRUE(pseudonymizationKey.has_value());
    const auto key = SafeString(std::move(*pseudonymizationKey));
    auto client = createClient();
    // pharmacist
    auto req = makeEncryptedRequest(
        HttpMethod::POST,
        "/Task/160.000.000.004.714.77/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
        jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
    req.setHeader(Header::XForwardedFor, "127.0.0.1");
    auto response = client.send(req);
    EXPECT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    ASSERT_TRUE(response.getHeader().hasHeader(Header::PnIpaddress));
    ASSERT_TRUE(response.getHeader().hasHeader(Header::PnTelematikId));
    const auto ipAddress =
        DtbpPseudonymization::decryptLogData(response.getHeader().header(Header::PnIpaddress).value(), key);
    EXPECT_EQ(ipAddress, "127.0.0.1");

    const auto telematikId =
        DtbpPseudonymization::decryptLogData(response.getHeader().header(Header::PnTelematikId).value(), key);
    EXPECT_EQ(telematikId, "X020406082");
}

TEST_F(VauRequestHandlerTest, utf8BomXml)
{
    A_28427.test("UTF-8 BOM before inner request body");
    auto client = createClient();
    const std::string bom{static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF)};
    const std::string body = bom + R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";
    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(body, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before inner request body");
}

TEST_F(VauRequestHandlerTest, utf8BomJson)
{
    A_28427.test("UTF-8 BOM before inner request body");
    auto client = createClient();
    const std::string bom{static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF)};
    const std::string body = bom + R"(
{
  "resourceType": "Parameters",
  "parameter": [
    {
      "name": "workflowType",
      "valueCoding": {
        "code": "160",
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"
      }
    }
  ]
}
)";
    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(body, "application/fhir+json"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before inner request body");
}

TEST_F(VauRequestHandlerTest, utf16LEBomXml)
{
    A_28427.test("UTF-16-LE BOM before inner request body");
    auto client = createClient();
    const std::u16string bom=u"\xFEFF";
    const std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    auto u16Body = bom + convert.from_bytes(body);
    std::string_view view{reinterpret_cast<char*>(u16Body.data()), u16Body.size() * 2};

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(view, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before inner request body");
}

TEST_F(VauRequestHandlerTest, utf16LEWithoutBomXml)
{
    A_28427.test("UTF-16-LE without BOM before inner request body");
    auto client = createClient();
    const std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    auto u16Body = convert.from_bytes(body);
    std::string_view view{reinterpret_cast<char*>(u16Body.data()), u16Body.size() * 2};

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(view, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "Input is not a FHIR+XML/UTF-8 document");
}

TEST_F(VauRequestHandlerTest, utf16LEBomJson)
{
    A_28427.test("UTF-16-LE BOM before inner request body");
    auto client = createClient();
    const std::u16string bom=u"\xFEFF";
    const std::string body = R"(
{
  "resourceType": "Parameters",
  "parameter": [
    {
      "name": "workflowType",
      "valueCoding": {
        "code": "160",
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"
      }
    }
  ]
}
)";

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    auto u16Body = bom + convert.from_bytes(body);
    std::string_view view{reinterpret_cast<char*>(u16Body.data()), u16Body.size() * 2};

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(view, "application/fhir+json"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before inner request body");
}

TEST_F(VauRequestHandlerTest, utf16LEWithoutBomJson)
{
    A_28427.test("UTF-16-LE without BOM before inner request body");
    auto client = createClient();
    const std::string body = R"(
{
  "resourceType": "Parameters",
  "parameter": [
    {
      "name": "workflowType",
      "valueCoding": {
        "code": "160",
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"
      }
    }
  ]
}
)";

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    auto u16Body = convert.from_bytes(body);
    std::string_view view{reinterpret_cast<char*>(u16Body.data()), u16Body.size() * 2};

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(view, "application/fhir+json"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "Input is not a FHIR+JSON/UTF-8 document");
}

TEST_F(VauRequestHandlerTest, utf16BEBom)
{
    A_28427.test("UTF-16-BE BOM before inner request body");
    auto client = createClient();
    std::array<unsigned char, 570> utf16be_txt = {
  0xFE, 0xFF, 0x00, 0x3c, 0x00, 0x50, 0x00, 0x61, 0x00, 0x72, 0x00, 0x61, 0x00, 0x6d,
  0x00, 0x65, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x73, 0x00, 0x20,
  0x00, 0x78, 0x00, 0x6d, 0x00, 0x6c, 0x00, 0x6e, 0x00, 0x73, 0x00, 0x3d,
  0x00, 0x22, 0x00, 0x68, 0x00, 0x74, 0x00, 0x74, 0x00, 0x70, 0x00, 0x3a,
  0x00, 0x2f, 0x00, 0x2f, 0x00, 0x68, 0x00, 0x6c, 0x00, 0x37, 0x00, 0x2e,
  0x00, 0x6f, 0x00, 0x72, 0x00, 0x67, 0x00, 0x2f, 0x00, 0x66, 0x00, 0x68,
  0x00, 0x69, 0x00, 0x72, 0x00, 0x22, 0x00, 0x3e, 0x00, 0x0d, 0x00, 0x0a,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c, 0x00, 0x70, 0x00, 0x61,
  0x00, 0x72, 0x00, 0x61, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x74, 0x00, 0x65,
  0x00, 0x72, 0x00, 0x3e, 0x00, 0x0d, 0x00, 0x0a, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c, 0x00, 0x6e,
  0x00, 0x61, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x20, 0x00, 0x76, 0x00, 0x61,
  0x00, 0x6c, 0x00, 0x75, 0x00, 0x65, 0x00, 0x3d, 0x00, 0x22, 0x00, 0x77,
  0x00, 0x6f, 0x00, 0x72, 0x00, 0x6b, 0x00, 0x66, 0x00, 0x6c, 0x00, 0x6f,
  0x00, 0x77, 0x00, 0x54, 0x00, 0x79, 0x00, 0x70, 0x00, 0x65, 0x00, 0x22,
  0x00, 0x2f, 0x00, 0x3e, 0x00, 0x0d, 0x00, 0x0a, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c, 0x00, 0x76,
  0x00, 0x61, 0x00, 0x6c, 0x00, 0x75, 0x00, 0x65, 0x00, 0x43, 0x00, 0x6f,
  0x00, 0x64, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x67, 0x00, 0x3e, 0x00, 0x0d,
  0x00, 0x0a, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c, 0x00, 0x73,
  0x00, 0x79, 0x00, 0x73, 0x00, 0x74, 0x00, 0x65, 0x00, 0x6d, 0x00, 0x20,
  0x00, 0x76, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x75, 0x00, 0x65, 0x00, 0x3d,
  0x00, 0x22, 0x00, 0x68, 0x00, 0x74, 0x00, 0x74, 0x00, 0x70, 0x00, 0x73,
  0x00, 0x3a, 0x00, 0x2f, 0x00, 0x2f, 0x00, 0x67, 0x00, 0x65, 0x00, 0x6d,
  0x00, 0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6b, 0x00, 0x2e, 0x00, 0x64,
  0x00, 0x65, 0x00, 0x2f, 0x00, 0x66, 0x00, 0x68, 0x00, 0x69, 0x00, 0x72,
  0x00, 0x2f, 0x00, 0x65, 0x00, 0x72, 0x00, 0x70, 0x00, 0x2f, 0x00, 0x43,
  0x00, 0x6f, 0x00, 0x64, 0x00, 0x65, 0x00, 0x53, 0x00, 0x79, 0x00, 0x73,
  0x00, 0x74, 0x00, 0x65, 0x00, 0x6d, 0x00, 0x2f, 0x00, 0x47, 0x00, 0x45,
  0x00, 0x4d, 0x00, 0x5f, 0x00, 0x45, 0x00, 0x52, 0x00, 0x50, 0x00, 0x5f,
  0x00, 0x43, 0x00, 0x53, 0x00, 0x5f, 0x00, 0x46, 0x00, 0x6c, 0x00, 0x6f,
  0x00, 0x77, 0x00, 0x54, 0x00, 0x79, 0x00, 0x70, 0x00, 0x65, 0x00, 0x22,
  0x00, 0x2f, 0x00, 0x3e, 0x00, 0x0d, 0x00, 0x0a, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x3c, 0x00, 0x63, 0x00, 0x6f, 0x00, 0x64, 0x00, 0x65,
  0x00, 0x20, 0x00, 0x76, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x75, 0x00, 0x65,
  0x00, 0x3d, 0x00, 0x22, 0x00, 0x31, 0x00, 0x36, 0x00, 0x30, 0x00, 0x22,
  0x00, 0x2f, 0x00, 0x3e, 0x00, 0x0d, 0x00, 0x0a, 0x00, 0x20, 0x00, 0x20,
  0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c, 0x00, 0x2f,
  0x00, 0x76, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x75, 0x00, 0x65, 0x00, 0x43,
  0x00, 0x6f, 0x00, 0x64, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x67, 0x00, 0x3e,
  0x00, 0x0d, 0x00, 0x0a, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x3c,
  0x00, 0x2f, 0x00, 0x70, 0x00, 0x61, 0x00, 0x72, 0x00, 0x61, 0x00, 0x6d,
  0x00, 0x65, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x3e, 0x00, 0x0d,
  0x00, 0x0a, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x50, 0x00, 0x61, 0x00, 0x72,
  0x00, 0x61, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72,
  0x00, 0x73, 0x00, 0x3e
};
    auto view = std::string_view{reinterpret_cast<char*>(utf16be_txt.data()), utf16be_txt.size()};
    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(view, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before inner request body");
}

TEST_F(VauRequestHandlerTest, utf8BomInSignedPrescription)
{
    auto task = createTask();
    A_28428.test("UTF-8 BOM in signed prescription");
    auto client = createClient();
    const std::string bom{static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF)};

    ResourceTemplates::KbvBundleOptions options{.prescriptionId = task.prescriptionId()};
    auto kbvBundleXml = bom + ResourceTemplates::kbvBundleXml(options);
    auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(std::string(kbvBundleXml), model::Timestamp::now());
    auto body = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +
                cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/" + options.prescriptionId.toString() + "/$activate",
                                    mJwtBuilder.makeJwtArzt(), task.accessCode(), std::make_pair(body, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before XML resource");
}

TEST_F(VauRequestHandlerTest, utf16LEBomInSignedPrescription)
{
    auto task = createTask();
    A_28428.test("UTF-16-LE BOM in signed prescription");
    auto client = createClient();
    const std::u16string bom=u"\xFEFF";

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    ResourceTemplates::KbvBundleOptions options{.prescriptionId = task.prescriptionId()};
    auto kbvBundleXml = bom + convert.from_bytes(ResourceTemplates::kbvBundleXml(options));
    auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(
        std::string(reinterpret_cast<char*>(kbvBundleXml.data()), kbvBundleXml.size() * 2), model::Timestamp::now());
    auto body = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +
                cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/" + options.prescriptionId.toString() + "/$activate",
                                    mJwtBuilder.makeJwtArzt(), task.accessCode(), std::make_pair(body, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "illegal BOM before XML resource");
}

TEST_F(VauRequestHandlerTest, utf16LEWithoutBomInSignedPrescription)
{
    auto task = createTask();
    A_28428.test("UTF-16-LE without BOM in signed prescription");
    auto client = createClient();

    std::wstring_convert<codecvt<char16_t,char,std::mbstate_t>, char16_t> convert;
    ResourceTemplates::KbvBundleOptions options{.prescriptionId = task.prescriptionId()};
    auto kbvBundleXml = convert.from_bytes(ResourceTemplates::kbvBundleXml(options));
    auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(
        std::string(reinterpret_cast<char*>(kbvBundleXml.data()), kbvBundleXml.size() * 2), model::Timestamp::now());
    auto body = R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +
                cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";

    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/" + options.prescriptionId.toString() + "/$activate",
                                    mJwtBuilder.makeJwtArzt(), task.accessCode(), std::make_pair(body, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "Input is not a FHIR+XML/UTF-8 document");
}

TEST_F(VauRequestHandlerTest, invalidUtf8Xml)
{
    A_28427.test("UTF-8 containing invalid code point");
    auto client = createClient();
    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";

    using namespace std::string_literals;
    // inject invalid byte 0b11011111.
    body = String::replaceAll(body, "160", "\xdf"s + "160");
    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(body, "application/fhir+xml"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "Input is not a FHIR+XML/UTF-8 document");
}

TEST_F(VauRequestHandlerTest, invalidUtf8Json)
{
    A_28427.test("UTF-8 containing invalid code point");
    auto client = createClient();
    std::string body = R"(
{
  "resourceType": "Parameters",
  "parameter": [
    {
      "name": "workflowType",
      "valueCoding": {
        "code": "160",
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"
      }
    }
  ]
}
)";

    using namespace std::string_literals;
    // inject invalid byte 0b11011111.
    body = String::replaceAll(body, "160", "\xdf"s + "160");
    auto req = makeEncryptedRequest(HttpMethod::POST, "/Task/$create", mJwtBuilder.makeJwtArzt(), {},
                                    std::make_pair(body, "application/fhir+json"));
    auto response = client.send(req);
    const ClientResponse innerResponse = mTeeProtocol.parseResponse(response);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    expectOperationOutcome(innerResponse, "Input is not a FHIR+JSON/UTF-8 document");
}

