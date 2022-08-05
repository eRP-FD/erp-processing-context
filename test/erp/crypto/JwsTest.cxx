/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/Jws.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Sha256.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/String.hxx"

#include "mock/crypto/MockCryptography.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <rapidjson/document.h>


class JwsTest : public testing::Test
{
public:
};

TEST_F(JwsTest, Roundtrip)// NOLINT
{
    JoseHeader header(JoseHeader::Algorithm::ES256);
    header.setContentType(MimeType::fhirJson);
    header.setType(MimeType::jose);
    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    header.setX509Certificate(cert);
    header.setCritical({"alg", "x5c"});

    std::string payload("the payload");
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});

    Jws jws(header, payload, privKey);
    auto serialized = jws.compactSerialized();

    auto parts = String::split(serialized, '.');
    ASSERT_EQ(parts.size(), 3);

    auto headerPart = Base64::decodeToString(parts[0]);
    auto payloadPart = Base64::decodeToString(parts[1]);
    auto signaturePart = Base64::decodeToString(parts[2]);

    rapidjson::Document headerDocument;
    headerDocument.Parse(headerPart);
    ASSERT_NE(rapidjson::Pointer("/alg").Get(headerDocument), nullptr);
    ASSERT_EQ(std::string(rapidjson::Pointer("/alg").Get(headerDocument)->GetString()), "ES256");
    ASSERT_EQ(rapidjson::Pointer("/jku").Get(headerDocument), nullptr);
    ASSERT_EQ(rapidjson::Pointer("/jwk").Get(headerDocument), nullptr);
    ASSERT_EQ(rapidjson::Pointer("/kid").Get(headerDocument), nullptr);
    ASSERT_EQ(rapidjson::Pointer("/x5u").Get(headerDocument), nullptr);
    ASSERT_NE(rapidjson::Pointer("/x5c").Get(headerDocument), nullptr);

    ASSERT_TRUE(rapidjson::Pointer("/x5c").Get(headerDocument)->IsArray());
    ASSERT_EQ(rapidjson::Pointer("/x5c").Get(headerDocument)->GetArray().Size(), static_cast<rapidjson::SizeType>(1));
    ASSERT_EQ(std::string(rapidjson::Pointer("/x5c").Get(headerDocument)->GetArray()[0].GetString()),
              cert.toBase64Der());

    ASSERT_EQ(rapidjson::Pointer("/x5t").Get(headerDocument), nullptr);
    ASSERT_NE(rapidjson::Pointer("/x5t#S256").Get(headerDocument), nullptr);
    ASSERT_EQ(std::string(rapidjson::Pointer("/x5t#S256").Get(headerDocument)->GetString()),
              Base64::toBase64Url(Base64::encode(Sha256::fromBin(cert.toBinaryDer()))));
    ASSERT_NE(rapidjson::Pointer("/typ").Get(headerDocument), nullptr);
    ASSERT_EQ(std::string(rapidjson::Pointer("/typ").Get(headerDocument)->GetString()), "application/jose");
    ASSERT_NE(rapidjson::Pointer("/cty").Get(headerDocument), nullptr);
    ASSERT_EQ(std::string(rapidjson::Pointer("/cty").Get(headerDocument)->GetString()), "application/fhir+json");
    ASSERT_NE(rapidjson::Pointer("/crit").Get(headerDocument), nullptr);

    ASSERT_TRUE(rapidjson::Pointer("/crit").Get(headerDocument)->IsArray());
    ASSERT_EQ(rapidjson::Pointer("/crit").Get(headerDocument)->GetArray().Size(), static_cast<rapidjson::SizeType>(2));
    ASSERT_EQ(std::string(rapidjson::Pointer("/crit").Get(headerDocument)->GetArray()[0].GetString()), "alg");
    ASSERT_EQ(std::string(rapidjson::Pointer("/crit").Get(headerDocument)->GetArray()[1].GetString()), "x5c");

    ASSERT_EQ(payloadPart, payload);

    auto ctx = shared_EVP_MD_CTX::make();
    ASSERT_EQ(EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, cert.getPublicKey().get()), 1);
    auto serializedHeader = header.serializeToBase64Url();
    ASSERT_EQ(EVP_DigestVerifyUpdate(ctx, serializedHeader.data(), serializedHeader.size()), 1);
    ASSERT_EQ(EVP_DigestVerifyUpdate(ctx, ".", 1), 1);
    auto payloadSerialized = Base64::toBase64Url(Base64::encode(payload));
    ASSERT_EQ(EVP_DigestVerifyUpdate(ctx, payloadSerialized.data(), payloadSerialized.size()), 1);
    std::array<unsigned char, 32> rvec{0};
    std::array<unsigned char, 32> svec{0};
    std::copy_n(std::begin(signaturePart), rvec.size(), std::begin(rvec));
    std::copy_n(std::begin(signaturePart) + rvec.size(), svec.size(), std::begin(svec));
    auto sig = shared_ECDSA_SIG::make();
    auto r = shared_BN::make();
    auto s = shared_BN::make();
    BN_bin2bn(rvec.data(), 32, r);
    BN_bin2bn(svec.data(), 32, s);
    ECDSA_SIG_set0(sig, r.release(), s.release());
    int siglen = i2d_ECDSA_SIG(sig, nullptr);
    std::vector<unsigned char> sig_der_bytes(static_cast<size_t>(siglen), 0u);
    unsigned char* rawptr = sig_der_bytes.data();
    i2d_ECDSA_SIG(sig, &rawptr);
    ASSERT_EQ(EVP_DigestVerifyFinal(ctx, sig_der_bytes.data(), gsl::narrow<size_t>(siglen)), 1);

    auto detachedSerialized = jws.compactDetachedSerialized();
    auto detachedParts = String::split(detachedSerialized, '.');
    ASSERT_EQ(detachedParts.size(), 3);
    ASSERT_EQ(detachedParts[0], parts[0]);
    ASSERT_TRUE(detachedParts[1].empty());
    ASSERT_EQ(detachedParts[2], parts[2]);
}
