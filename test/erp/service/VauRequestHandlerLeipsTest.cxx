/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ServerTestBase.hxx"

#include <gtest/gtest.h>
#include <test/util/EnvironmentVariableGuard.hxx>

#include "test_config.h"

class VauRequestHandlerLeipsTest : public ServerTestBase
{
public:
    explicit VauRequestHandlerLeipsTest(bool forceMockDatabase = false)
        : ServerTestBase(forceMockDatabase)
    {
    }

    static constexpr int timespan_ms = 4000;
    EnvironmentVariableGuard calls{"ERP_TOKEN_ULIMIT_CALLS", "5"};
    EnvironmentVariableGuard timespan{"ERP_TOKEN_ULIMIT_TIMESPAN_MS", std::to_string(timespan_ms)};
    EnvironmentVariableGuard check{"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS", "2"};
    EnvironmentVariableGuard refresh{"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS", "6"};
    EnvironmentVariableGuard enab{"ERP_REPORT_LEIPS_KEY_ENABLE", "true"};
};

TEST_F(VauRequestHandlerLeipsTest, Leips)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string claimString =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/jwt/claims_patient.json");
    rapidjson::Document claim;
    ASSERT_NO_THROW(claim.Parse(claimString));
    JWT token;
    ASSERT_NO_THROW(token = mJwtBuilder.getJWT(claim));
    auto client = createClient();
    auto response = client.send(makeEncryptedRequest(HttpMethod::GET, "/Task", token));
    ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
    // Despite that REPORT_KEY_ENABLE is true the VauRequestHandler does not set the header field because
    // GET /Task is performed with oid == versicherter:
    EXPECT_FALSE(response.getHeader().hasHeader(Header::InnerRequestLeips));
}

TEST_F(VauRequestHandlerLeipsTest, Leips1)//NOLINT(readability-function-cognitive-complexity)
{
    mMockDatabase->fillWithStaticData();
    auto client = createClient();
    {
        // pharmacist
        auto encryptedRequest = makeEncryptedRequest(
            HttpMethod::POST,
            "/Task/160.000.000.004.714.77/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea",
            jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke));
        auto response = client.send(encryptedRequest);
        ASSERT_FALSE(response.getHeader().hasHeader(Header::VAUErrorCode));
        EXPECT_TRUE(response.getHeader().hasHeader(Header::InnerRequestLeips));
    }
}
