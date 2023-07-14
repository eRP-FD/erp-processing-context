/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/enrolment/EnrolmentHelper.hxx"

#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmIdentity.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"

#include "mock/enrolment/MockEnrolmentManager.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include "mock/mock_config.h"

#include <gtest/gtest.h>


/**
 * The MockEnrolmentManager is essentially a tool to allow more complex tests. Therefore the tests in this
 * file are as simple as possible.
 */
class MockEnrolmentManagerTest : public testing::Test
{
public:
    void SetUp (void) override
    {
        if (Configuration::instance().getOptionalStringValue(ConfigurationKey::HSM_DEVICE, "").empty())
        {
            TVLOG(0) << "no HSM device configured => skipping test";
            GTEST_SKIP();
        }
        if (MockConfiguration::instance().getOptionalBoolValue(MockConfigurationKey::MOCK_USE_MOCK_TPM, true))
        {
            TVLOG(0) << "use of mock TPM is configured ON => skipping test";
            GTEST_SKIP();
        }
    };
};


TEST_F(MockEnrolmentManagerTest, successfulSetupAndTeeToken)
{
    // Create blob cache without adding any blobs.
    auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>());

    // Use TPM and HSM (simulators) to setup the necessary blobs for TEE token negotiation.
    TpmProxyDirect tpm (*blobCache);
    MockEnrolmentManager::createAndStoreAkEkAndQuoteBlob(tpm, *blobCache);

    const auto teeToken = EnrolmentHelper(HsmIdentity::getWorkIdentity()).createTeeToken(*blobCache);
    TVLOG(1) << "got tee token : " << Base64::encode(teeToken.data);
    ASSERT_NE(teeToken.data.size(), 0);
}


TEST_F(MockEnrolmentManagerTest, failForMissingBlobs)
{
    // Create blob cache without adding any blobs.
    auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>());

    // Do NOT set up the necessary blobs.

    ASSERT_ANY_THROW(
        EnrolmentHelper(HsmIdentity::getWorkIdentity()).createTeeToken(*blobCache));
}

class TestEnrolmentHelper : public EnrolmentHelper
{
public:
    using EnrolmentHelper::EnrolmentHelper;
    enum SimulatedError
    {
        NonceBlobExpiration,
        None
    };
    ErpBlob createTeeTokenWithSimulatedError (
        TpmProxy& tpm,
        BlobCache& blobCache,
        SimulatedError simulatedError)
    {
        const uint32_t teeGeneration = 0;

        const auto trustedAk = blobCache.getBlob(BlobType::AttestationPublicKey);
        const auto trustedQuote = blobCache.getBlob(BlobType::Quote);
        const auto [teeTokenNonceVector, teeTokenNonceBlob] = getNonce(teeGeneration);

        if (simulatedError == NonceBlobExpiration)
        {
            TVLOG(0) << "sleeping for 7 minutes to force the nonce blob to expire";
            std::this_thread::sleep_for(std::chrono::minutes(7));
            TVLOG(0) << "waking up after 7 minute sleep. Expecting the next getQuote call to notifyFailure";
        }

        const auto teeQuote = getQuote(tpm, teeTokenNonceVector, trustedQuote.getPcrSet().toPcrList(), "ERP_ATTESTATION");
              auto teeToken = getTeeToken(trustedAk.getAkName(), teeQuote, trustedAk.blob, teeTokenNonceBlob, trustedQuote.blob);

        return teeToken;
    }
};


/**
 * Verify that a nonce blob expires after 6 minutes. Its use after that time results in an error.
 * This test is disabled because
 * a) it runs a little longer (slightly over 7 minutes) and
 * b) it requires a special setup:
 *    make sure that the following services are reachable, they can run locally or port forwarded from e.g. DEV
 *    - enrolment api on port 9191
 *    - hsm on port 3001, hsm has to be built with the `DISABLE_BLOB_EXPIRY` option removed in vau-hsm/firmware/CMakeLists.tx
 *    - tpm on port 2321, this requires option `#tss:with_hardware_tpm=True` in conanfile.txt to be removed or commented out
 */
TEST_F(MockEnrolmentManagerTest, DISABLED_createTeeToken_failForExpiredNonceBlob)
{
    // Create blob cache without adding any blobs.
    auto blobCache = std::make_shared<BlobCache>(std::make_unique<MockBlobDatabase>());

    // Use TPM and HSM (simulators) to setup the necessary blobs for TEE token negotiation.
    TpmProxyDirect tpm (*blobCache);
    MockEnrolmentManager::createAndStoreAkEkAndQuoteBlob(tpm, *blobCache, std::string(MOCK_DATA_DIR) + "/enrolment/cacertecc.crt");

    auto enrolmentHelper = TestEnrolmentHelper(HsmIdentity::getWorkIdentity());
    // Expect this test to throw an exception for a b101001e error code.
    ASSERT_ANY_THROW(
        enrolmentHelper.createTeeTokenWithSimulatedError(tpm, *blobCache, TestEnrolmentHelper::SimulatedError::NonceBlobExpiration));
}
