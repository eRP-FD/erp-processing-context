/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/Jwt.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Jws.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/JwtException.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/String.hxx"

#include "mock/crypto/MockCryptography.hxx"
#include "test_config.h"
#include "test/util/JwtBuilder.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <chrono>


using namespace std::chrono_literals;

namespace
{
    std::string getSignature (const JoseHeader& header, const std::string& payloadBase64)
    {
        const auto jws = Jws(
            header,
            Base64::decodeToString(payloadBase64),
            MockCryptography::getIdpPrivateKey());
        const auto serialized = jws.compactSerialized();
        const auto lastDot = serialized.rfind('.');
        return serialized.substr(lastDot + 1);
    }
}

class JwtTest : public testing::Test
{
public:
    void SetUp() override
    {
        mIdpPublicKey = MockCryptography::getIdpPublicKey();
        mIdpPrivateKey = MockCryptography::getIdpPrivateKey();
    }

    const JoseHeader mJoseHeader = JoseHeader(JoseHeader::Algorithm::BP256R1);

    // Header and payload is base64-encoded. The signature was
    // generated with a local certificate (private key) whose public
    // key is extracted and used for the verification.
    const std::string mHeader = mJoseHeader.serializeToBase64Url();
    const std::string mPayload{
        "eyJzdWIiOiJzdWJqZWN0Iiwib3JnYW5pemF0aW9uTmFtZSI6ImdlbWF0aWsgR21iSCBOT1QtVkFMSUQiLCJwcm9mZX"
        "NzaW9uT0lEIjoiMS4yLjI3Ni4wLjc2LjQuNDkiLCJpZE51bW1lciI6IlgxMTQ0Mjg1MzAiLCJpc3MiOiJzZW5kZXIi"
        "LCJyZXNwb25zZV90eXBlIjoiY29kZSIsImNvZGVfY2hhbGxlbmdlX21ldGhvZCI6IlMyNTYiLCJnaXZlbl9uYW1lIj"
        "oiSnVuYSIsImNsaWVudF9pZCI6bnVsbCwiYXVkIjoiZXJwLnplbnRyYWwuZXJwLnRpLWRpZW5zdGUuZGUiLCJhY3Ii"
        "OiJlaWRhcy1sb2EtaGlnaCIsInNjb3BlIjoib3BlbmlkIGUtcmV6ZXB0Iiwic3RhdGUiOiJhZjBpZmpzbGRraiIsIn"
        "JlZGlyZWN0X3VyaSI6bnVsbCwiZXhwIjoxNjAzMTk3NjUyLCJmYW1pbHlfbmFtZSI6IkZ1Y2hzIiwiY29kZV9jaGFs"
        "bGVuZ2UiOm51bGwsImlhdCI6MTYwMzE5NzM1MiwiYXV0aF90aW1lIjoxNjAzMTk3MzUyfQ"};
    // Character at position 5 changed from d to 4.
    const std::string mTamperedPayload{
        "eyJz4WIiOiJzdWJqZWN0Iiwib3JnYW5pemF0aW9uTmFtZSI6ImdlbWF0aWsgR21iSCBOT1QtVkFMSUQiLCJwcm9mZX"
        "NzaW9uT0lEIjoiMS4yLjI3Ni4wLjc2LjQuNDkiLCJpZE51bW1lciI6IlgxMTQ0Mjg1MzAiLCJpc3MiOiJzZW5kZXIi"
        "LCJyZXNwb25zZV90eXBlIjoiY29kZSIsImNvZGVfY2hhbGxlbmdlX21ldGhvZCI6IlMyNTYiLCJnaXZlbl9uYW1lIj"
        "oiSnVuYSIsImNsaWVudF9pZCI6bnVsbCwiYXVkIjoiZXJwLnplbnRyYWwuZXJwLnRpLWRpZW5zdGUuZGUiLCJhY3Ii"
        "OiJlaWRhcy1sb2EtaGlnaCIsInNjb3BlIjoib3BlbmlkIGUtcmV6ZXB0Iiwic3RhdGUiOiJhZjBpZmpzbGRraiIsIn"
        "JlZGlyZWN0X3VyaSI6bnVsbCwiZXhwIjoxNjAzMTk3NjUyLCJmYW1pbHlfbmFtZSI6IkZ1Y2hzIiwiY29kZV9jaGFs"
        "bGVuZ2UiOm51bGwsImlhdCI6MTYwMzE5NzM1MiwiYXV0aF90aW1lIjoxNjAzMTk3MzUyfQ"};
    const std::string mSignature = getSignature(mJoseHeader, mPayload);

    shared_EVP_PKEY mIdpPublicKey;
    shared_EVP_PKEY mIdpPrivateKey;
};

TEST_F(JwtTest, ValidVerification)//NOLINT(readability-function-cognitive-complexity)
{
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mPayload + "." + mSignature));
    ASSERT_NE(jwt, nullptr);
    ASSERT_NO_THROW(jwt->verifySignature(mIdpPublicKey));
}

TEST_F(JwtTest, InvalidSignatureVerification)//NOLINT(readability-function-cognitive-complexity)
{
    A_19131.test("Unit test for VauRequestHandler::verify");
    std::unique_ptr<JWT> jwt = nullptr;
    // Skip the first char of signature in order to make it invalid. It could be any other char.
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mPayload + "." + mSignature.substr(1)));
    ASSERT_NE(jwt, nullptr);
    ASSERT_THROW(jwt->verifySignature(mIdpPublicKey), JwtInvalidSignatureException);
}

TEST_F(JwtTest, MissingSignatureVerification)//NOLINT(readability-function-cognitive-complexity)
{
    A_19131.test("Unit test for VauRequestHandler::verify");
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mPayload + "."));
    ASSERT_THROW(jwt->verifySignature(mIdpPublicKey), JwtInvalidSignatureException);
}

TEST_F(JwtTest, TamperedPayloadVerification)//NOLINT(readability-function-cognitive-complexity)
{
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mTamperedPayload + "." + mSignature));
    ASSERT_NE(jwt, nullptr);
    ASSERT_THROW(jwt->verifySignature(mIdpPublicKey), JwtException);
}

TEST_F(JwtTest, DefaultConstructor)
{
    A_19993_01.test("Check for the two periods in a JWT.");
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>());
    ASSERT_NE(jwt, nullptr);
    ASSERT_EQ(jwt->serialize(), "..");
    A_19993_01.finish();
}

TEST_F(JwtTest, HeaderChecks)//NOLINT(readability-function-cognitive-complexity)
{
    A_20362.test("Check for the two periods in a JWT.");

    const auto idpPublicKey = MockCryptography::getIdpPublicKey();

    std::string header = Base64::encode("{}");
    std::string payload = Base64::encode("{}");
    std::string signature = Base64::encode("invalid");
    JWT jwt0(header + "." + payload + "." + signature);
    ASSERT_THROW(jwt0.verify(idpPublicKey), JwtInvalidRfcFormatException); // header has no alg key.

    header = Base64::encode(R"({"alg":"none"})");
    payload = Base64::encode("{}");
    signature = Base64::encode("invalid");
    JWT jwt1(header + "." + payload + "." + signature);
    ASSERT_THROW(jwt1.verify(idpPublicKey), JwtInvalidSignatureException);

    header = Base64::encode(R"({"alg":"BP256R1"})");
    payload = Base64::encode("{}");
    signature = Base64::encode("invalid");
    JWT jwt2(header + "." + payload + "." + signature);
    // Test must fail at claims check, header is valid and must not throw JwtInvalidFormatException
    ASSERT_THROW(jwt2.verify(idpPublicKey), JwtRequiredClaimException);

    A_20362.finish();
}

TEST_F(JwtTest, KnownStringClaim)//NOLINT(readability-function-cognitive-complexity)
{
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mPayload + "." + mSignature));
    ASSERT_NE(jwt, nullptr);
    const auto given_name = jwt->stringForClaim(JWT::givenNameClaim).value_or("");
    const auto family_name = jwt->stringForClaim(JWT::familyNameClaim).value_or("");
    const auto organizationName = jwt->stringForClaim(JWT::organizationNameClaim).value_or("");
    ASSERT_EQ(given_name, "Juna");
    ASSERT_EQ(family_name, "Fuchs");
    ASSERT_EQ(organizationName, "gematik GmbH NOT-VALID");
}

TEST_F(JwtTest, UnknownStringClaim)
{
    std::unique_ptr<JWT> jwt = nullptr;
    ASSERT_NO_THROW(jwt = std::make_unique<JWT>(mHeader + "." + mPayload + "." + mSignature));
    ASSERT_NE(jwt, nullptr);
    ASSERT_EQ(jwt->stringForClaim("___"), std::optional<std::string>{});
}

TEST_F(JwtTest, RequiredClaimsAvailable)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    A_20369_01.test("Unit test for required claims availability.");
    ASSERT_NO_THROW(jwt.checkRequiredClaims());
    A_20369_01.finish();
}

TEST_F(JwtTest, MissingRequiredClaim)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    claim.RemoveMember(std::string{JWT::displayNameClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    A_20369_01.test("Unit test for required claims availability.");
    ASSERT_THROW(jwt.checkRequiredClaims(), JwtRequiredClaimException);
    A_20369_01.finish();
}

TEST_F(JwtTest, MissingOptionalClaims)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_arzt.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    claim.RemoveMember(std::string{JWT::displayNameClaim});
    claim.RemoveMember(std::string{JWT::familyNameClaim});
    claim.RemoveMember(std::string{JWT::givenNameClaim});
    claim.RemoveMember(std::string{JWT::organizationNameClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    A_20369_01.test("Unit test for required claims availability.");
    ASSERT_NO_THROW(jwt.checkRequiredClaims());
    A_20369_01.finish();
}

TEST_F(JwtTest, InvalidJsonPayload)
{
    ASSERT_THROW(JWT (mHeader + "." + Base64::encode(" a : b }") + "." + mSignature), JwtInvalidFormatException);
}

TEST_F(JwtTest, Expired)
{
    A_20373.test("Expect token validation fail with exp in the past");
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() - 5s);
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() - 10s);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_THROW(jwt.verify(mIdpPublicKey), JwtExpiredException);
}

TEST_F(JwtTest, ExpiredChecksIat)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    claim.RemoveMember(std::string{JWT::nbfClaim});

    // Set iat slightly above the allowed threshold, which is 2 seconds.
    const auto now = std::chrono::system_clock::now();
    const auto exp_iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::iatClaim}].SetInt64( (exp_iat + JWT::A20373_iatClaimToleranceSeconds + 1s).count() );
    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_THROW(jwt.verify(mIdpPublicKey), JwtExpiredException);

    // Set iat to the maximum allowed threshold, which is 2 seconds.
    claim[std::string{JWT::iatClaim}].SetInt64( (exp_iat + JWT::A20373_iatClaimToleranceSeconds).count() );
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_NO_THROW(jwt.verify(mIdpPublicKey));
}

TEST_F(JwtTest, NotExpired)//NOLINT(readability-function-cognitive-complexity)
{
    A_20373.test("Test token with validity between now and now + 5s");
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    claim.RemoveMember(std::string{JWT::nbfClaim});

    const auto now = std::chrono::system_clock::now();
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5s);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_NO_THROW(jwt.verify(mIdpPublicKey));
}

TEST_F(JwtTest, IssuedAtInFuture)//NOLINT(readability-function-cognitive-complexity)
{
    A_20373.test("Expect token validation fail with iat in future");
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    // Forcefully set iat in the future. An exception must be signalled, because the JWT is issued for later use.
    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5s);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_THROW(jwt.verify(mIdpPublicKey), JwtExpiredException);
}

TEST_F(JwtTest, IssuedAtToleranceExceed)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    // Set iat one second above the tolerated difference to cause the expiration check failure.
    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>( (now.time_since_epoch() + JWT::A20373_iatClaimToleranceSeconds + 1s) );
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_THROW(jwt.verify(mIdpPublicKey), JwtExpiredException);
}

TEST_F(JwtTest, MaxAgeExceedWithNbf)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    A_20374.test("Test that the age check is really disabled when nbf is provided.");
    // The iat is exactly 1 second too old (max. five minutes) to be considered as young enough ...
    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5min);
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() - 5min - 1s);
    const auto nbf = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 1min);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    // ... DESPITE that we specify an nbf claim:
    claim[std::string{JWT::nbfClaim}].SetInt64(nbf.count());

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    ASSERT_THROW(jwt.verify(mIdpPublicKey), JwtExpiredException);
    A_20374.finish();
}

TEST_F(JwtTest, InvalidClaimsTypes)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5min);
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());

    A_20370.test("Unit test for valid claims data types.");
    claim[std::string{JWT::displayNameClaim}] = 0xAA55;

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    ASSERT_THROW(jwt.checkRequiredClaims(), JwtInvalidFormatException);
    A_20370.finish();
}

TEST(JwtRoundtripTest, success)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_arzt.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    JwtBuilder builder{MockCryptography::getIdpPrivateKey()};
    for (size_t i = 0; i < 1500; ++i)
    {
        JWT jwt;
        ASSERT_NO_THROW(jwt = builder.getJWT(claim));
        ASSERT_NO_THROW(jwt.verifySignature(MockCryptography::getIdpPublicKey()));
    }
}

TEST_F(JwtTest, ForceMissingRequiredTimingClaim_exp)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
    // A token according to A_20369-01 must provide exp and iat. The JWT class handles this explicitly. Nevertheless, it is
    // good to test the case when a token with an absent exp/iat is passed. This is tested here, by removing the exp/iat key:
    claim.RemoveMember(std::string{JWT::expClaim});
    // Also remove the optional nbf claim to be really sure that no other timestamp might interfere.
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    // Missing exp must lead to an expired exception:
    ASSERT_THROW(jwt.checkIfExpired(), JwtExpiredException);
    // Double check that the iat claim was really removed:
    ASSERT_THROW(jwt.checkRequiredClaims(), JwtRequiredClaimException);
}

TEST_F(JwtTest, ForceMissingRequiredTimingClaim_iat)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + 5min);
    claim[std::string{JWT::expClaim}].SetInt64(exp.count());

    // A token according to A_20369-01 must provide technically required claims (such as exp and iat).
    // The JWT class handles this explicitly. Nevertheless, it is
    // good to test the case when a token with an absent exp/iat is passed. This is tested here, by removing the exp/iat key:
    claim.RemoveMember(std::string{JWT::iatClaim});
    // Also remove the optional nbf claim to be really sure that no other timestamp might interfere.
    claim.RemoveMember(std::string{JWT::nbfClaim});

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));
    // Missing iat causes no exception when checking for expiration. In the past, iat had an age
    // check which caused an exception when not provided. The problem is catched later, due to
    // absent required claim.
    ASSERT_NO_THROW(jwt.checkIfExpired());
    // Double check that the iat claim was really removed:
    ASSERT_THROW(jwt.checkRequiredClaims(), JwtRequiredClaimException);
}

TEST_F(JwtTest, ConstructorChecks)//NOLINT(readability-function-cognitive-complexity)
{
    // Basic  constructor checks.
    EXPECT_NO_THROW(JWT());
    EXPECT_THROW(JWT(""), JwtInvalidFormatException);
    EXPECT_THROW(JWT("."), JwtInvalidFormatException);
    EXPECT_THROW(JWT(".."), JwtInvalidFormatException);
    EXPECT_THROW(JWT("a."), JwtInvalidFormatException);
    EXPECT_THROW(JWT("a.."), JwtInvalidFormatException);
    EXPECT_THROW(JWT("a.b"), JwtInvalidFormatException);
    EXPECT_THROW(JWT("a.b."), JwtInvalidFormatException);
    EXPECT_THROW(JWT("a.b.c"), JwtInvalidFormatException);

    {
        // All problamtic JWTs from above are handled during verification.
        EXPECT_THROW(JWT().verify(mIdpPublicKey), JwtInvalidRfcFormatException);
        EXPECT_THROW(JWT("").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT(".").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT("..").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT("a..").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT("a.b").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT("a.b.").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT("a.b.c").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT(mHeader + ".b.c").verify(mIdpPublicKey), JwtInvalidFormatException);
        EXPECT_THROW(JWT(mHeader + "." + Base64::encode("{}") + ".c").verify(mIdpPublicKey), JwtRequiredClaimException);

        const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

        // The iat claim has no age check as before, thus no expiration exception is thrown (until 2050). Now the signature fails,
        // due to an invalid signature:
        EXPECT_THROW(JWT(mHeader + "." + Base64::encode(claimString) + ".c").verify(mIdpPublicKey), JwtInvalidSignatureException);

        rapidjson::Document claim;
        ASSERT_NO_THROW(claim.Parse(claimString));

        const auto now_epoch_secs =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
        const auto iat = now_epoch_secs;
        const auto exp = now_epoch_secs + 30s;
        claim[std::string{JWT::expClaim}].SetInt64(exp.count());
        claim[std::string{JWT::iatClaim}].SetInt64(iat.count());
        claim.RemoveMember(std::string{JWT::nbfClaim});

        JwtBuilder builder{mIdpPrivateKey};
        JWT jwt;
        ASSERT_NO_THROW(jwt = builder.getJWT(claim));

        auto parts = String::split(jwt.serialize(), ".");
        EXPECT_THROW(JWT(parts[0] + "." + parts[1] + ".").verify(mIdpPublicKey), JwtInvalidSignatureException);
        EXPECT_THROW(JWT(parts[0] + "." + parts[1] + ".c").verify(mIdpPublicKey), JwtInvalidSignatureException);
        EXPECT_THROW(JWT(parts[0] + "." + parts[1] + "." + Base64::encode("{}")).verify(mIdpPublicKey), JwtInvalidSignatureException);

        EXPECT_NO_THROW(JWT(jwt).verify(mIdpPublicKey));
    }
}

TEST_F(JwtTest, CheckAudClaim)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    A_21520.test("Check tokens aud claim");
    claim[std::string{JWT::audClaim}].SetString("http://");

    JwtBuilder builder{mIdpPrivateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claim));

    ASSERT_THROW(jwt.checkAudClaim(), JwtInvalidAudClaimException);
    A_21520.finish();
}

TEST_F(JwtTest, OptionalClaimsForNonInsurant)//NOLINT(readability-function-cognitive-complexity)
{
    const auto idpPublicKey = MockCryptography::getIdpPublicKey();
    JwtBuilder builder{mIdpPrivateKey};

    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");

    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));

    const auto now = std::chrono::system_clock::now();
    const auto exp_iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claim[std::string{JWT::expClaim}].SetInt64(exp_iat.count());
    claim[std::string{JWT::iatClaim}].SetInt64(exp_iat.count());
    claim.RemoveMember(std::string{JWT::nbfClaim});

    rapidjson::Pointer givenName{"/" + std::string{JWT::givenNameClaim}};
    rapidjson::Pointer lastName{"/" + std::string{JWT::familyNameClaim}};
    rapidjson::Pointer organizationName{"/" + std::string{JWT::organizationNameClaim}};

    givenName.Set(claim, rapidjson::Value());
    lastName.Set(claim, rapidjson::Value());
    organizationName.Set(claim, rapidjson::Value());

    JWT jwt;
    EXPECT_NO_THROW(jwt = builder.getJWT(claim));
    EXPECT_THROW(jwt.verify(idpPublicKey), JwtInvalidFormatException);

    JWT arztJwt;
    rapidjson::Pointer professionOid{"/" + std::string{JWT::professionOIDClaim}};
    professionOid.Set(claim, std::string{profession_oid::oid_arzt});
    EXPECT_NO_THROW(arztJwt = builder.getJWT(claim));
    EXPECT_NO_THROW(arztJwt.verify(idpPublicKey));
}

TEST_F(JwtTest, displayNamePatient)
{
    A_19391_01.test("extract the display name for insurants");
    JwtBuilder builder{mIdpPrivateKey};
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Pointer givenName{"/" + std::string{JWT::givenNameClaim}};
    rapidjson::Pointer lastName{"/" + std::string{JWT::familyNameClaim}};

    // old variant without display name claim
    {
        rapidjson::Document claim;
        ASSERT_NO_THROW(claim.Parse(claimString));
        claim.RemoveMember(std::string{JWT::displayNameClaim});
        givenName.Set(claim, "Max");
        lastName.Set(claim, "Mustermann");

        const auto jwt = builder.getJWT(claim);
        ASSERT_NO_THROW(jwt.checkRequiredClaims());
        EXPECT_EQ(jwt.displayName(), "Max Mustermann");
    }

    // only display name claim
    {
        rapidjson::Document claim;
        ASSERT_NO_THROW(claim.Parse(claimString));
        claim[std::string{JWT::displayNameClaim}].SetString("Herr Mustermann");

        const auto jwt = builder.getJWT(claim);
        ASSERT_NO_THROW(jwt.checkRequiredClaims());
        EXPECT_EQ(jwt.displayName(), "Herr Mustermann");
    }

    // both claim variants, preferring the new one
    {
        rapidjson::Document claim;
        ASSERT_NO_THROW(claim.Parse(claimString));
        claim[std::string{JWT::displayNameClaim}].SetString("Herr Mustermann");
        givenName.Set(claim, "Max");
        lastName.Set(claim, "Mustermann");

        const auto jwt = builder.getJWT(claim);
        ASSERT_NO_THROW(jwt.checkRequiredClaims());
        EXPECT_EQ(jwt.displayName(), "Herr Mustermann");
    }

    // missing both
    {
        rapidjson::Document claim;
        ASSERT_NO_THROW(claim.Parse(claimString));
        claim.RemoveMember(std::string{JWT::displayNameClaim});
        const auto jwt = builder.getJWT(claim);
        ASSERT_ANY_THROW(jwt.checkRequiredClaims());
        ASSERT_FALSE(jwt.displayName().has_value());
    }
}

TEST_F(JwtTest, displayNameOrganization)
{
    A_19391_01.test("extract the display name for organizations");
    JwtBuilder builder{mIdpPrivateKey};
    const std::string claimString = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_apotheke.json");
    rapidjson::Document claim;

    ASSERT_NO_THROW(claim.Parse(claimString));
    rapidjson::Pointer displayName{"/" + std::string{JWT::displayNameClaim}};
    displayName.Set(claim, "Herr Mustermann");
    claim[std::string{JWT::organizationNameClaim}].SetString("Arzt");

    const auto jwt = builder.getJWT(claim);
    EXPECT_EQ(jwt.displayName(), "Arzt");
}
