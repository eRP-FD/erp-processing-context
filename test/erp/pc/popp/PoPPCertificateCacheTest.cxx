/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPCertificateVerifier.hxx"
#include "shared/crypto/EllipticCurve.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/Base64.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <boost/algorithm/string/replace.hpp>


namespace
{

/// Build a minimal JWKS JWT payload containing the given key entries (raw JSON).
/// Signature is not checked by invalidateAndUpdate, so any signing key works.
PoPPCertificateVerifier::Jwks makeJwks(const std::string& keysJson)
{
    const std::string payload = R"({"keys":[)" + keysJson + "]}";
    return PoPPCertificateVerifier::Jwks{JwtBuilder::testBuilder().getJWT(payload)};
}

/// Load the DER-encoded certificate at `resourcePath` and return it as
/// a base64 string suitable for the x5c field.
std::string certBase64(const std::string& resourcePath)
{
    const auto der = ResourceManager::instance().getStringResource(resourcePath);
    const auto cert = X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(der.data()), der.size()});
    return cert.toBase64();
}

/// Build a JSON key entry for the JWKS payload.
/// `kid`  — key identifier string
/// `cert` — base64-encoded DER certificate (x5c value)
/// `x`,`y`— base64url-encoded public key coordinates
std::string keyEntry(const std::string& kid, const std::string& cert,
                     const std::string& x, const std::string& y)
{
    auto tpl = ResourceManager::instance().getStringResource("test/popp/Jwks.key.json");
    boost::replace_all(tpl, "##KID##", kid);
    boost::replace_all(tpl, "##CERT##", cert);
    boost::replace_all(tpl, "##X##", x);
    boost::replace_all(tpl, "##Y##", y);
    return tpl;
}

/// Derive base64url x and y coordinates from a DER certificate's public key.
std::pair<std::string, std::string> coordsFromCertDer(const std::string& der)
{
    const auto cert = X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(der.data()), der.size()});
    auto pk = shared_EVP_PKEY::make(cert.getPublicKey());
    const auto components =
        EllipticCurveUtils::getPaddedXYComponents(pk, EllipticCurve::KeyCoordinateLength);
    pk.release();
    return {Base64::toBase64Url(Base64::encode(components.x)),
            Base64::toBase64Url(Base64::encode(components.y))};
}

} // namespace


class PoPPCertificateCacheTest : public ::testing::Test
{
protected:
    static constexpr auto kCertPath1 =
        "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig_ec/popp_zd_sig_ec.der";
    static constexpr auto kCertPath2 =
        "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig2_ec/popp_zd_sig2_ec.der";

        void SetUp() override
    {
        mDer1 = ResourceManager::instance().getStringResource(kCertPath1);
        mDer2 = ResourceManager::instance().getStringResource(kCertPath2);

        mBase64Cert1 = certBase64(kCertPath1);
        mBase64Cert2 = certBase64(kCertPath2);

        auto [x1, y1] = coordsFromCertDer(mDer1);
        mX1 = x1; mY1 = y1;

        auto [x2, y2] = coordsFromCertDer(mDer2);
        mX2 = x2; mY2 = y2;
    }

    /// Build a single key JWKS with `kid` and certificate 1.
    PoPPCertificateVerifier::Jwks jwksWithKey1(const std::string& kid = "kid-1")
    {
        return makeJwks(keyEntry(kid, mBase64Cert1, mX1, mY1));
    }

    /// Build a single key JWKS with `kid` and certificate 2.
    PoPPCertificateVerifier::Jwks jwksWithKey2(const std::string& kid = "kid-2")
    {
        return makeJwks(keyEntry(kid, mBase64Cert2, mX2, mY2));
    }

    /// Build a two key JWKS.
    PoPPCertificateVerifier::Jwks jwksWithBothKeys()
    {
        return makeJwks(keyEntry("kid-1", mBase64Cert1, mX1, mY1) + "," +
                        keyEntry("kid-2", mBase64Cert2, mX2, mY2));
    }

    PoPPCertificateVerifier::PoPPCertificateCache mCache;

    std::string mDer1;
    std::string mDer2;
    std::string mBase64Cert1;
    std::string mBase64Cert2;
    std::string mX1;
    std::string mY1;
    std::string mX2;
    std::string mY2;
};


TEST_F(PoPPCertificateCacheTest, InsertNewKey)
{
    // Cache is empty. After update it must contain kid-1.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));

    const auto key = mCache.getKey("kid-1");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->kid, "kid-1");
    EXPECT_TRUE(key->certificate);
    EXPECT_TRUE(key->publicKey);
    EXPECT_FALSE(key->lastSuccessAt.has_value()); // not yet TSL-verified
}

TEST_F(PoPPCertificateCacheTest, InsertTwoNewKeys)
{
    mCache.invalidateAndUpdate(jwksWithBothKeys());

    EXPECT_TRUE(mCache.getKey("kid-1").has_value());
    EXPECT_TRUE(mCache.getKey("kid-2").has_value());
    EXPECT_EQ(mCache.getKeys().size(), 2u);
}

TEST_F(PoPPCertificateCacheTest, InsertDoesNotTouchUnrelatedExistingKey)
{
    // Initialize with kid-1, then update with a JWKS that also contains kid-1.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));
    // Simulate a successful TSL check so lastSuccessAt gets a value.
    mCache.mCache.at("kid-1").lastSuccessAt = model::Timestamp::now();

    // Run update again with the same JWKS. kid-1 must remain unchanged.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));

    const auto key = mCache.getKey("kid-1");
    ASSERT_TRUE(key.has_value());
    // lastSuccessAt must still be set (existing entry was not overwritten.)
    EXPECT_TRUE(key->lastSuccessAt.has_value());
}

TEST_F(PoPPCertificateCacheTest, NoChangeWhenSameKidAndSameCertificate)
{
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));
    // Simulate a successful TSL check.
    mCache.mCache.at("kid-1").lastSuccessAt = model::Timestamp::now();
    const auto timestampBefore = mCache.mCache.at("kid-1").lastSuccessAt;

    // Identical JWKS, no change.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));

    const auto key = mCache.getKey("kid-1");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->lastSuccessAt, timestampBefore);
}

TEST_F(PoPPCertificateCacheTest, ChangeWhenKidExistsWithDifferentCertificate)
{
    // Insert kid-1 with cert-1.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));
    mCache.mCache.at("kid-1").lastSuccessAt = model::Timestamp::now();

    // JWKS now advertises kid-1 with cert-2 (fingerprint changed).
    const auto jwksKid1WithCert2 = makeJwks(keyEntry("kid-1", mBase64Cert2, mX2, mY2));
    mCache.invalidateAndUpdate(jwksKid1WithCert2);

    // Entry must not survive (replaced) because the certificate changed.
    const auto key = mCache.getKey("kid-1");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->lastSuccessAt, std::nullopt);
    // The stored certificate must be cert-2.
    EXPECT_EQ(key->certificate->getSha256FingerprintHex(),
              X509Certificate::createFromAsnBytes(
                  {reinterpret_cast<const unsigned char*>(mDer2.data()), mDer2.size()})
                  .getSha256FingerprintHex());
}

TEST_F(PoPPCertificateCacheTest, StaleKeyIsRemovedWhenAbsentFromJwks)
{
    // Insert two keys.
    mCache.invalidateAndUpdate(jwksWithBothKeys());
    ASSERT_TRUE(mCache.getKey("kid-1").has_value());
    ASSERT_TRUE(mCache.getKey("kid-2").has_value());

    // Update with a JWKS that only contains kid-2. kid-1 must be removed.
    mCache.invalidateAndUpdate(jwksWithKey2("kid-2"));

    EXPECT_FALSE(mCache.getKey("kid-1").has_value());
    EXPECT_TRUE(mCache.getKey("kid-2").has_value());
}

TEST_F(PoPPCertificateCacheTest, AllKeysRemovedWhenJwksContainsDifferentKids)
{
    // Insert kid-1.
    mCache.invalidateAndUpdate(jwksWithKey1("kid-1"));
    ASSERT_TRUE(mCache.getKey("kid-1").has_value());

    // Update with a completely different kid — kid-1 must be evicted.
    mCache.invalidateAndUpdate(jwksWithKey2("kid-new"));

    EXPECT_FALSE(mCache.getKey("kid-1").has_value());
    EXPECT_TRUE(mCache.getKey("kid-new").has_value());
}

TEST_F(PoPPCertificateCacheTest, KeyWithMissingX5cIsSkippedAndNotInserted)
{
    // A key entry without a certificate (x5c is the empty placeholder inserted
    // by parseKey when the field is absent) must be silently skipped.
    const std::string keyNoX5c =
        R"({"kty":"EC","crv":"P-256","kid":"no-cert","x":")" + mX1 +
        R"(","y":")" + mY1 + R"("})";

    const PoPPCertificateVerifier::Jwks jwks = [&]{
        const std::string payload = R"({"keys":[)" + keyNoX5c + "]}";
        return PoPPCertificateVerifier::Jwks{JwtBuilder::testBuilder().getJWT(payload)};
    }();

    // Should not throw and should not insert anything.
    EXPECT_NO_THROW(mCache.invalidateAndUpdate(jwks));
    EXPECT_FALSE(mCache.getKey("no-cert").has_value());
    EXPECT_TRUE(mCache.getKeys().empty());
}

// Ensure and a cache update with more than one key preserves the lastSuccessAt also for
// the second entry
TEST_F(PoPPCertificateCacheTest, SecondPassRunsAfterAllKeysCollected_LastSuccessAtPreserved)
{
    // Populate the cache with both keys and simulate successful TSL checks for both.
    mCache.invalidateAndUpdate(jwksWithBothKeys());
    const auto ts = model::Timestamp::now();
    mCache.mCache.at("kid-1").lastSuccessAt = ts;
    mCache.mCache.at("kid-2").lastSuccessAt = ts;

    // Re-run with the same two-key JWKS.  Both fingerprints are unchanged, so
    // invalidateAndUpdate must leave both entries untouched.
    mCache.invalidateAndUpdate(jwksWithBothKeys());

    // With the bug, the second pass ran after processing kid-1 alone: allKeys only
    // contained "kid-1" at that point, so kid-2 was erased (lastSuccessAt lost).
    const auto key1 = mCache.getKey("kid-1");
    const auto key2 = mCache.getKey("kid-2");

    ASSERT_TRUE(key1.has_value()) << "kid-1 must still be in the cache";
    ASSERT_TRUE(key2.has_value()) << "kid-2 must still be in the cache";

    EXPECT_TRUE(key1->lastSuccessAt.has_value()) << "kid-1 lastSuccessAt must be preserved";
    EXPECT_TRUE(key2->lastSuccessAt.has_value()) << "kid-2 lastSuccessAt must be preserved";
}
