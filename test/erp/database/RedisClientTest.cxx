/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/RedisClient.hxx"

#include "shared/crypto/Jwt.hxx"
#include "erp/database/redis/RateLimiter.hxx"
#include "shared/util/JwtException.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test_config.h"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>


using namespace std::chrono_literals;

class RedisClientTest : public testing::Test
{
};

TEST_F(RedisClientTest, BasicTest)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::chrono;
    RateLimiter dosHandler(std::make_unique<MockRedisStore>(), "ERP-PC-DOS",
					gsl::narrow<size_t>(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS, 100)),
					std::chrono::milliseconds(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS, 1000)));

    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();

    const auto& claimOrig = ResourceManager::instance().getJsonResource(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claims;
    claims.CopyFrom(claimOrig, claims.GetAllocator());
    claims.RemoveMember(std::string{JWT::nbfClaim});

    // Configure for 10 calls within 4 s.
    dosHandler.setTimespan(4s);
    dosHandler.setUpperLimitCalls(10);

    // JWT expires in 10 s from now. Use JWT 10 times within 4 s in order to block further access.
    // Retry one call with the same JWT after at most 10 s.
    const auto now = system_clock::now();
    const auto exp = time_point_cast<seconds>(now + 10s).time_since_epoch();
    const auto iat = time_point_cast<seconds>(now).time_since_epoch();
    claims[std::string{JWT::expClaim}].SetInt64(exp.count());
    claims[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claims[std::string{JWT::subClaim}].SetString("attacker");

    JwtBuilder builder{privateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claims));
    ASSERT_NO_THROW(jwt.verify(publicKey));

    auto exp_ms = time_point<system_clock, milliseconds>(seconds(exp));

    // Fire 10 calls as fast as possible (which is within the upper limit specified above.)
    for (int i = 0; i < 10; i++)
    {
        EXPECT_TRUE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));
    }
    // Eleventh call must be denied (constraint is max 10 calls within 1 second.)
    EXPECT_FALSE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));

    // Force pause until jwt lifetime is expired.
    std::this_thread::sleep_for(11s);

    // Redis Cleanup kicks in which means the token can be used again.
    EXPECT_TRUE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));

    // BUT: it's verification will fail because lifetime is expired.
    ASSERT_THROW(jwt.verify(publicKey), JwtExpiredException);
}

TEST_F(RedisClientTest, BasicTest2)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::chrono;
    std::shared_ptr<MockRedisStore> redis = std::make_unique<MockRedisStore>();
    RateLimiter dosHandler(redis, "ERP-PC-DOS",
					gsl::narrow<size_t>(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_CALLS, 100)),
					std::chrono::milliseconds(Configuration::instance().getOptionalIntValue(ConfigurationKey::TOKEN_ULIMIT_TIMESPAN_MS, 1000)));

    const auto privateKey = MockCryptography::getIdpPrivateKey();
    const auto publicKey = MockCryptography::getIdpPublicKey();

    const auto& claimOrig = ResourceManager::instance().getJsonResource(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claims;
    claims.CopyFrom(claimOrig, claims.GetAllocator());
    claims.RemoveMember(std::string{JWT::nbfClaim});

    // Configure for 10 calls within 4 s.
    dosHandler.setTimespan(4s);
    dosHandler.setUpperLimitCalls(10);

    // JWT expires in 10 s from now. Use JWT 10 times within 1 s in order to block further access.
    // Retry one call with the same JWT after at most 10 s.
    const auto now = system_clock::now();
    const auto exp = time_point_cast<seconds>(now + 10s).time_since_epoch();
    const auto iat = time_point_cast<seconds>(now).time_since_epoch();
    claims[std::string{JWT::expClaim}].SetInt64(exp.count());
    claims[std::string{JWT::iatClaim}].SetInt64(iat.count());
    claims[std::string{JWT::subClaim}].SetString("attacker");

    JwtBuilder builder{privateKey};
    JWT jwt;
    ASSERT_NO_THROW(jwt = builder.getJWT(claims));
    ASSERT_NO_THROW(jwt.verify(publicKey));

    auto exp_ms = time_point<system_clock, milliseconds>(seconds(exp));

    // Fire 10 calls as fast as possible (which is within the upper limit specified above.)
    for (int i = 0; i < 10; i++)
    {
        EXPECT_TRUE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));
    }

    // Eleventh call must be denied (constraint is max 10 calls within 1 second.)
    EXPECT_FALSE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));

    // Force pause until at least timespan is over. Test that the token is still blocked.
    std::this_thread::sleep_for(2s);
    EXPECT_FALSE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));

    // Force pause until jwt lifetime is expired.
    std::this_thread::sleep_for(11s);

    // Redis removed the blocklisted key due to its expiration, thus pass.
    EXPECT_TRUE(dosHandler.updateCallsCounter(jwt.stringForClaim(JWT::subClaim).value(), exp_ms));

    // BUT: it's verification will fail because lifetime is expired.
    ASSERT_THROW(jwt.verify(publicKey), JwtExpiredException);
}
