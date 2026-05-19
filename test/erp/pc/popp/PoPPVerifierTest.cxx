/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPVerifier.hxx"
#include "PoPPTokenBuilder.hxx"
#include "erp/pc/popp/PoPPToken.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/Health.hxx"
#include "shared/model/ModelException.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/SafeString.hxx"
#include "test/erp/pc/popp/PoPPCertificateVerifierServiceMock.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <fmt/color.h>
#include <gtest/gtest.h>

class PoPPVerifierTest : public ::testing::Test
{

public:
    PoPPVerifierTest()
    {
        setupDefaultMock(poPpCertificateVerifierServiceMock);
    }

    PoPPCertificateVerifier::EntityStatement entityStatement(const std::string& sub = "https://erp-mock-idp:8443")
    {
        std::string str = R"({
  "iss": "https://popp-ru-dev-interim.int.epa.rise-link.de:8443",
  "sub": "##SUB##",
  "iat": 1772008100,
  "exp": 1772094500,
  "authority_hints": [
    "https://app-ref.federationmaster.de"
  ],
  "metadata": {
    "federation_entity": {
      "organization_name": "RISE GmbH"
    },
    "oauth_resource": {
      "signed_jwks_uri": "https://popp-ru-dev-interim.int.epa.rise-link.de:8443/.well-known/signed-jwks"
    }
  },
  "jwks": {
    "keys": [
      {
        "kty": "EC",
        "crv": "P-256",
        "x": "KyMGY6R4m157nbgaMofed3Q4U9zP_GPNPM8-QNNPPaY",
        "y": "MXkEvTogUDXfGQXeNIaYRp6Lfr3n6JoFgzyaJ1aRPoA",
        "kid": "cVu2V_jsRaG9WAs4UfsDIZmGaJFkbBq5fNKXAdhSG2A",
        "alg": "ES256",
        "use": "sig"
      }
    ]
  }
})";
        boost::replace_all(str, "##SUB##", sub);
        return PoPPCertificateVerifier::EntityStatement{JwtBuilder::testBuilder().getJWT(str)};
    }

    void checkFailure(const PoPPToken& poppToken, const PoPPCertificateVerifier::EntityStatement& entityStatement,
                      const JWT& accessToken, const std::string& expectedMessage)
    {
        try
        {
            const PoPPVerifier verifier{};
            verifier.verifyTokenSignature(poPpCertificateVerifierServiceMock, poppToken);
            verifier.verifyTokenClaims(poppToken, entityStatement, accessToken);
            FAIL() << "Expected exception";
        }
        catch (const model::ModelException& e)
        {
            EXPECT_EQ(e.what(), expectedMessage);
        }
    }

    PoPPCertificateVerifierServiceMock poPpCertificateVerifierServiceMock{};
};

// GEMREQ-start A_26450#valid
TEST_F(PoPPVerifierTest, VerifySampleSuccess)
{
    auto accessToken = JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678");
    const PoPPVerifier verifier{};
    auto poppToken = PoPPTokenBuilder{}.getToken();
    EXPECT_NO_THROW(verifier.verifyTokenSignature(poPpCertificateVerifierServiceMock, poppToken));
    EXPECT_NO_THROW(verifier.verifyTokenClaims(poppToken, entityStatement(), accessToken));
}
// GEMREQ-end A_26450#valid

// GEMREQ-start A_26450#wrongTyp
TEST_F(PoPPVerifierTest, VerifyFailWrongTyp)
{
    // GEMREQ-start A_26450#valid
    A_26450.test("wrong typ");
    auto poppToken = PoPPTokenBuilder{}.withTyp("wrongTyp").getToken();
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "Unexpected PoPP token type: wrongTyp");
}
// GEMREQ-end A_26450#wrongTyp

// GEMREQ-start A_26450#wrongAlg
TEST_F(PoPPVerifierTest, VerifyFailWrongAlg)
{
    A_26450.test("wrong alg");
    auto poppToken = PoPPTokenBuilder{}.withAlg(JoseHeader::Algorithm::BP256R1).getToken();
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "Unexpected PoPP token algorithm: BP256R1");
}
// GEMREQ-end A_26450#wrongAlg

// GEMREQ-start A_26450#signature
TEST_F(PoPPVerifierTest, VerifyFailSignature)
{
    A_26450.test("signature");
    const SafeString prvPem{std::string{ResourceManager::instance().getStringResource(
        "test/generated_pki/sub_ca1_ec/certificates/qes_cert1_ec/qes_cert1_ec_key.pem")}};
    auto wrongKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(prvPem);
    auto poppToken = PoPPTokenBuilder{}.getToken(wrongKey);
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "Verification failed - invalid signature or payload.");
}
// GEMREQ-end A_26450#signature

// GEMREQ-start A_26452#wrongIss
TEST_F(PoPPVerifierTest, VerifyFailWrongIss)
{
    A_26452.test("wrong iss");
    auto poppToken = PoPPTokenBuilder{}.withIss("wrong iss").getToken();
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "PoPPToken.iss must contain the URL of the PoPP-Service from entity statement sub attribute: wrong "
                 "iss!=https://erp-mock-idp:8443");
}
// GEMREQ-end A_26452#wrongIss

// GEMREQ-start A_26452#wrongIat
TEST_F(PoPPVerifierTest, VerifyFailWrongIat)
{
    A_26452.test("wrong iat");
    A_23399_01.test("iat too old");
    using namespace std::chrono_literals;
    auto poppToken = PoPPTokenBuilder{}.withIat((model::Timestamp::now() - 31min).toTimeT()).getToken();
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "PoPPToken.iat is older than 30 minutes");
}
// GEMREQ-end A_26452#wrongIat

// GEMREQ-start A_26452#wrongActorId
TEST_F(PoPPVerifierTest, VerifyFailWrongActorId)
{
    A_26452.test("wrong actorId");
    A_23402_01.test("wrong actorId");
    auto poppToken = PoPPTokenBuilder{}.withActorId("wrong-actorId").getToken();
    checkFailure(poppToken, entityStatement(), JwtBuilder::testBuilder().makeJwtApotheke("1-2012345678"),
                 "PoPPToken.actorId does not match ACCESS_TOKEN.idNumber");
}
// GEMREQ-end A_26452#wrongActorId