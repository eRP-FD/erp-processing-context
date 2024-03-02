/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test_config.h"

#include <gtest/gtest.h>

namespace {

JWT originalJWT { // NOLINT
    "eyJhbGciOiJCUDI1NlIxIn0.eyJhY3IiOiJlaWRhcy1sb2EtaGlnaCIsImF1ZCI6Imh0dHBzOi8vZXJ"
    "wLnRlbGVtYXRpay5kZS9sb2dpbiIsImV4cCI6MjUyNDYwODAwMCwiZmFtaWx5X25hbWUiOiJkZXIgTm"
    "FjaG5hbWUiLCJnaXZlbl9uYW1lIjoiZGVyIFZvcm5hbWUiLCJpYXQiOjE1ODUzMzY5NTYsImlkTnVtb"
    "WVyIjoiWDIzNDU2Nzg5MCIsImlzcyI6Imh0dHBzOi8vaWRwMS50ZWxlbWF0aWsuZGUvand0IiwianRp"
    "IjoiPElEUD5fMDEyMzQ1Njc4OTAxMjM0NTY3ODkiLCJuYmYiOjE1ODUzMzY5NTYsIm5vbmNlIjoiZnV"
    "1IGJhciBiYXoiLCJvcmdhbml6YXRpb25OYW1lIjoiSW5zdGl0dXRpb25zLSBvZGVyIE9yZ2FuaXNhdG"
    "lvbnMtQmV6ZWljaG51bmciLCJwcm9mZXNzaW9uT0lEIjoiMS4yLjI3Ni4wLjc2LjQuNDkiLCJzdWIiO"
    "iJSYWJjVVN1dVdLS1pFRUhtcmNObV9rVURPVzEzdWFHVTVaazhPb0J3aU5rIn0.VCoXYIPdOdlLfM7G"
    "Cc9i0NEIDMcK-5jFMFf_KHEK7bPoOZ_PhLHOn5i-LzQ2Ey0SDEBYAb3gkktOL5t_GOF-sOO4Gl_sKCd"
    "hiR2Pwi-t7X_JEtt5dTyS7_5mGPvkROLP"
};

JWT newJWT { // NOLINT
    "eyJhbGciOiJCUDI1NlIxIn0.eyJzdWIiOiJzdWJqZWN0Iiwib3JnYW5pemF0aW9uTmFtZSI6ImdlbWF"
    "0aWsgR21iSCBOT1QtVkFMSUQiLCJwcm9mZXNzaW9uT0lEIjoiMS4yLjI3Ni4wLjc2LjQuNDkiLCJpZE"
    "51bW1lciI6IlgxMTQ0Mjg1MzAiLCJpc3MiOiJzZW5kZXIiLCJyZXNwb25zZV90eXBlIjoiY29kZSIsI"
    "mNvZGVfY2hhbGxlbmdlX21ldGhvZCI6IlMyNTYiLCJnaXZlbl9uYW1lIjoiSnVuYSIsImNsaWVudF9p"
    "ZCI6bnVsbCwiYXVkIjoiZXJwLnplbnRyYWwuZXJwLnRpLWRpZW5zdGUuZGUiLCJhY3IiOiJlaWRhcy1"
    "sb2EtaGlnaCIsInNjb3BlIjoib3BlbmlkIGUtcmV6ZXB0Iiwic3RhdGUiOiJhZjBpZmpzbGRraiIsIn"
    "JlZGlyZWN0X3VyaSI6bnVsbCwiZXhwIjoxNjAzMTk3NjUyLCJmYW1pbHlfbmFtZSI6IkZ1Y2hzIiwiY"
    "29kZV9jaGFsbGVuZ2UiOm51bGwsImlhdCI6MTYwMzE5NzM1MiwiYXV0aF90aW1lIjoxNjAzMTk3MzUy"
    "fQ.XqPmrlF-6elvj6sAU0mH2GmBoggef-RYpTdJ3Ae9KiB3n7yvc3W27wH9hcTm4gSbdddNZ1_oZfP_"
    "Rc-U2Jb9Sg"
};

struct TestParam
{
    std::string testFile;
    const JWT& accessToken;
    enum SkipOrRun
    {
        SKIP, RUN
    };
    SkipOrRun skipOrRun;
};

inline std::ostream& operator << (std::ostream& out, const TestParam& param)
{
    out << R"({ "testFile": ")" << param.testFile << R"(" })";
    return out;
}
}

class ErpTeeProtocolTest : public ::testing::TestWithParam<TestParam>
{
public:
    void SetUp() override
    {
    }
};

TEST_P(ErpTeeProtocolTest, gematik_ref_key_config) // NOLINT
{
    // this test assumes that the key is configured in 02_development.config.json
    // the key is intentionally not hard coded as it is supposed to test
    // that the key is correctly configured
    // The key is located in: https://github.com/gematik/ref-eRp-FD-Server/blob/master/vau/src/decrypt.rs
    if (GetParam().skipOrRun == TestParam::SKIP)
    {
        GTEST_SKIP();
    }

    const std::string X_0123456789ABCDEF0123456789ABCDEF{
            static_cast<char>(0x01), static_cast<char>(0x23), static_cast<char>(0x45), static_cast<char>(0x67),
            static_cast<char>(0x89), static_cast<char>(0xAB), static_cast<char>(0xCD), static_cast<char>(0xEF),
            static_cast<char>(0x01), static_cast<char>(0x23), static_cast<char>(0x45), static_cast<char>(0x67),
            static_cast<char>(0x89), static_cast<char>(0xAB), static_cast<char>(0xCD), static_cast<char>(0xEF)
    };


    using namespace std::string_view_literals;
    using namespace std::string_literals;

    const auto& rootPath = std::string(TEST_DATA_DIR) + "/EllipticCurveUtilsTest/"s;

    const auto& cipher = FileHelper::readFileAsString(rootPath + GetParam().testFile);

    // Create a mocked blob cache and fill it with the bare minimal set of blobs (for this test fixture).
    auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>());
    const auto& vauKeyPair = MockCryptography::getEciesPrivateKeyPem();
    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.name = ErpVector::create("ecies");
    entry.blob = ErpBlob(vauKeyPair, 1);
    blobCache->storeBlob(std::move(entry));

    HsmPool hsm(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());

    auto innerTeeRequest = ErpTeeProtocol::decrypt(cipher, hsm);
    innerTeeRequest.parseHeaderAndBody();
    EXPECT_EQ(innerTeeRequest.version(), "1"sv);
    EXPECT_EQ(innerTeeRequest.authenticationToken().serialize(), GetParam().accessToken.serialize());
    EXPECT_EQ(innerTeeRequest.requestId(), X_0123456789ABCDEF0123456789ABCDEF);
    EXPECT_EQ(static_cast<std::string_view>(innerTeeRequest.aesKey()), X_0123456789ABCDEF0123456789ABCDEF);
    EXPECT_EQ(innerTeeRequest.header().method(), HttpMethod::POST);
    EXPECT_EQ(innerTeeRequest.header().target(), "/Task/$create"sv);
    EXPECT_EQ(innerTeeRequest.header().version(), Header::Version_1_1);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader(Header::ContentType));
    EXPECT_EQ(innerTeeRequest.header().header(Header::ContentType).value(), "application/fhir+json"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader(Header::UserAgent));
    EXPECT_EQ(innerTeeRequest.header().header(Header::UserAgent).value(), "PostmanRuntime/7.26.2"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader("Accept"));
    EXPECT_EQ(innerTeeRequest.header().header("Accept").value(), "*/*"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader("Postman-Token"));
    EXPECT_EQ(innerTeeRequest.header().header("Postman-Token").value(), "a3720e83-ef04-450a-bc46-21439551dff6"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader(Header::Host));
    EXPECT_EQ(innerTeeRequest.header().header(Header::Host).value(), "localhost:3000"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader("Accept-Encoding"));
    EXPECT_EQ(innerTeeRequest.header().header("Accept-Encoding").value(), "gzip, deflate, br"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader("Connection"));
    EXPECT_EQ(innerTeeRequest.header().header("Connection").value(), Header::KeepAlive);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader(Header::ContentLength));
    EXPECT_EQ(innerTeeRequest.header().header(Header::ContentLength).value(), "231"sv);
    EXPECT_TRUE(innerTeeRequest.header().hasHeader(Header::Authorization));
    EXPECT_EQ(innerTeeRequest.header().header(Header::Authorization), "Bearer " + GetParam().accessToken.serialize());
    EXPECT_EQ(innerTeeRequest.header().contentLength(), 231u);
    EXPECT_EQ(innerTeeRequest.header().contentType(), "application/fhir+json"sv);
    // Unknown for Requests:
    EXPECT_EQ(innerTeeRequest.header().status(), HttpStatus::Unknown);
    EXPECT_EQ(innerTeeRequest.header().keepAlive(), true);
}

TEST_F(ErpTeeProtocolTest, decrypt)// NOLINT(readability-function-cognitive-complexity)
{
    const auto cipher = ::std::string{TEST_DATA_DIR} + "/EllipticCurveUtilsTest/task_create.cipher";

    auto blobCache = ::std::make_shared<BlobCache>(::std::make_unique<MockBlobDatabase>());
    const auto& vauKeyPair = ::MockCryptography::getEciesPrivateKeyPem();
    blobCache->storeBlob(::BlobDatabase::Entry{::BlobType::EciesKeypair,
                                               ::ErpVector::create("ecies-1"),
                                               ::ErpBlob{vauKeyPair, 1},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {}});

    HsmPool hsm{::std::make_unique<::HsmMockFactory>(::std::make_unique<::HsmMockClient>(), blobCache),
                ::TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), ::std::make_shared<::Timer>()};

    EXPECT_NO_THROW(::ErpTeeProtocol::decrypt(::FileHelper::readFileAsString(cipher), hsm));

    // generated with "openssl ecparam -genkey -name brainpoolP256r1"
    ::SafeString wrongKey{R"(-----BEGIN EC PRIVATE KEY-----
MHgCAQEEIF4M9ZJ46/S5yOot0RDU1rJrdM6bmk+tOSJ3gCXsqi/+oAsGCSskAwMC
CAEBB6FEA0IABG/H/oLbhVc7AkESgRnC7V2RT5Ll1Mn89x03e08hu5vOdN7fcwWi
Qa4UBnlfJIUiGp3wlQm5wop/n1m3NCvHXTs=
-----END EC PRIVATE KEY-----)"};
    blobCache->storeBlob(::BlobDatabase::Entry{::BlobType::EciesKeypair,
                                               ::ErpVector::create("ecies-2"),
                                               ::ErpBlob{wrongKey, 1},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {}});

    EXPECT_NO_THROW(::ErpTeeProtocol::decrypt(::FileHelper::readFileAsString(cipher), hsm));


    blobCache->storeBlob(::BlobDatabase::Entry{::BlobType::EciesKeypair,
                                               ::ErpVector::create("ecies-3"),
                                               ::ErpBlob{wrongKey, 1},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {},
                                               {}});

    EXPECT_ANY_THROW(::ErpTeeProtocol::decrypt(::FileHelper::readFileAsString(cipher), hsm));

    blobCache->deleteBlob(::BlobType::EciesKeypair, ::ErpVector::create("ecies-1"));
    blobCache->deleteBlob(::BlobType::EciesKeypair, ::ErpVector::create("ecies-2"));
    blobCache->deleteBlob(::BlobType::EciesKeypair, ::ErpVector::create("ecies-3"));
}

INSTANTIATE_TEST_SUITE_P(gematik_ref_key_config, ErpTeeProtocolTest,
        testing::Values(
// The test files come from: https://github.com/gematik/ref-eRp-FD-Server/tree/master/server/examples
                             TestParam{"task_create.cipher", originalJWT, TestParam::SKIP},
// This test is partially handcrafted
// the JWT was taken from access_tokes.rs in the reference implemenration and patched into task_create.plain
// then the new tool vau_encrypt was used to generate the cipher file
                             TestParam{"task_create-ERP-3107.cipher", newJWT, TestParam::RUN}
));
