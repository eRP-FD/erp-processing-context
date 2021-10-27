/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tpm/Tpm.hxx"
#include "erp/tpm/TpmProduction.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>


/**
 * The tests in this file are only executed when a real or simulated TPM is available and configured.
 * Do that by disabling ERP_MOCK_USE_MOCK_TPM (set it to FALSE).
 */
class TpmProductionTest : public testing::Test
{
public:
    TpmProductionTest (void)
        : mTpmDeviceGuard("TPM_INTERFACE_TYPE", "socsim") // Workaround for a TPM bug in version 0.8.
    {
    }

    virtual void SetUp (void) override
    {
        if (MockConfiguration::instance().getOptionalBoolValue(MockConfigurationKey::MOCK_USE_MOCK_TPM, true))
            GTEST_SKIP();
    }

private:
    EnvironmentVariableGuard mTpmDeviceGuard;
};


/**
 * Enable this test when you have a TPM simulator or a hardware TPM configured.
 */
TEST_F(TpmProductionTest, createInstance)
{
    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    std::unique_ptr<Tpm> tpm;

    // Verify that the constructor of TpmProduction, which indirectly creates the attestation keypair does not trigger
    // an exception in the tpm client.
    ASSERT_NO_THROW(
         tpm = TpmProduction::createInstance(*blobCache));
}


TEST_F(TpmProductionTest, getAttestationKey)
{
    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    std::unique_ptr<Tpm> tpm = TpmProduction::createInstance(*blobCache);

    auto attestationKey = tpm->getAttestationKey(true);

    ASSERT_FALSE(attestationKey.publicKeyBase64.empty());
    ASSERT_FALSE(attestationKey.akNameBase64.empty());

    // Verify that the attestation key pair has found its way into the blob cache.
    ASSERT_NO_THROW(
        blobCache->getBlob(BlobType::AttestationKeyPair));
}


TEST_F(TpmProductionTest, getAttestationKey_failForMissingAkKeyPair)
{
    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    std::unique_ptr<Tpm> tpm = TpmProduction::createInstance(*blobCache);

    // Simulate a call outside enrolment. The attestation key pair blob is expected not to be created.
    ASSERT_ANY_THROW(
        tpm->getAttestationKey(false));

    // Verify that the attestation key pair has found its way into the blob cache.
    ASSERT_ANY_THROW(
        blobCache->getBlob(BlobType::AttestationKeyPair));
}


/**
 * This test runs a lot of calls in a lot of threads against the TPM.
 * Its primary purpose is to stress test the "there-can-only-be-a-single-instance" rule. Well, a single instance of
 * a) the anonymous class LockedTpmClient
 * b) which has a connected session to the TPM.
 * As the connection happens in the constructor of LockedTpmClient, technically there can be more than one instance of it.
 *
 */
TEST_F(TpmProductionTest, getAttestationKey_getAttestationKey_manyThreads)
{
    const size_t threadCount = 100;
    const size_t callCount = 100;

    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    const auto ak = TpmProduction::createInstance(*blobCache)->getAttestationKey(true);

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (size_t index=0; index<threadCount; ++index)
    {
        threads.emplace_back([&]
        {
            for (size_t innerIndex = 0; innerIndex<callCount; ++innerIndex)
            {
                std::unique_ptr<Tpm> tpm = TpmProduction::createInstance(*blobCache, -1);
                const auto innerAk = tpm->getAttestationKey(true);
                ASSERT_EQ(innerAk.akNameBase64, ak.akNameBase64);
                ASSERT_EQ(innerAk.publicKeyBase64, ak.publicKeyBase64);
            }
        });
    }

    for (auto& thread : threads)
        thread.join();
}
