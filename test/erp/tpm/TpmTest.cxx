/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tpm/Tpm.hxx"
#include "erp/tpm/TpmProduction.hxx"
#include "erp/util/Base64.hxx"
#include "mock/tpm/TpmTestData.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include <gtest/gtest.h>
#include <memory>


/**
 * Note that the processing context is not supposed to interpret or process data to and from the TPM other
 * than base64 encoding and decoding. Therefore tests can not do much beyond checking that output
 * data is base64 encoded and not empty.
 *
 * Static test data is used as described on https://dth01.ibmgcloud.net/confluence/pages/viewpage.action?spaceKey=ERP&title=JSON+Interface.
 */

namespace
{
    struct TestDescriptor
    {
        std::function<std::unique_ptr<Tpm>(BlobCache&)> tpmFactory;
        std::unique_ptr<TpmTestHelper> testHelper;
        bool isSimulated = false;
    };
    using TestDescriptorFactory = std::function<TestDescriptor(void)>;

    TestDescriptorFactory createSimulatedTestDescriptorFactory (void)
    {
        return []
        {
            TestDescriptor descriptor;
            descriptor.tpmFactory = [](BlobCache& blobCache){return TpmProduction::createInstance(blobCache);};
            descriptor.testHelper = TpmTestHelper::createTestHelperForProduction();
            descriptor.isSimulated = true;
            return descriptor;
        };
    }

    TestDescriptorFactory createMockTestDescriptorFactory (void)
    {
        return []
        {
            TestDescriptor descriptor;
            descriptor.tpmFactory = [](BlobCache&){return TpmTestHelper::createMockTpm();};
            descriptor.testHelper = TpmTestHelper::createTestHelperForMock();
            descriptor.isSimulated = false;
            return descriptor;
        };
    }
}


class TpmTest : public testing::TestWithParam<TestDescriptorFactory>
{
public:
    void SetUp (void) override
    {
        try
        {
            TestDescriptor descriptor = GetParam()();
            blobCache = MockBlobDatabase::createBlobCache (
                descriptor.isSimulated
                    ? MockBlobCache::MockTarget::SimulatedHsm
                    : MockBlobCache::MockTarget::MockedHsm);
            tpmFactory = std::move(descriptor.tpmFactory);
            testHelper = std::move(descriptor.testHelper);

            if (!tpmFactory || testHelper==nullptr)
                GTEST_SKIP();
        }
        catch(const std::exception& e)
        {
            // An exception at this place means that contacting the TPM or login to it has failed. That is a sign
            // that the TPM is not present or not properly set up => skip the test.
            TLOG(ERROR) << "caught exception '" << e.what() << "' while contacting the TPM, skipping test";
            GTEST_SKIP();
        }
    }

    Tpm& getTpm ()
    {
        if (tpm == nullptr)
            tpm = tpmFactory(*blobCache);
        return *tpm;
    }
    TpmTestHelper& getTestHelper() //NOLINT[readability-make-member-function-const]
    {
        return *testHelper;
    }


    std::shared_ptr<BlobCache> blobCache;
    std::unique_ptr<Tpm> tpm;
    std::function<std::unique_ptr<Tpm>(BlobCache&)> tpmFactory;
    std::unique_ptr<TpmTestHelper> testHelper;
};


TEST_P(TpmTest, getEndorsementKey)
{
    const auto ek = getTpm().getEndorsementKey();

    ASSERT_FALSE(Base64::decode(ek.certificateBase64).empty());
    ASSERT_FALSE(Base64::decode(ek.ekNameBase64).empty());
}


TEST_P(TpmTest, getAttestationKey)
{
    const auto ak = getTpm().getAttestationKey();

    ASSERT_FALSE(Base64::decode(ak.publicKeyBase64).empty());
    ASSERT_FALSE(Base64::decode(ak.akNameBase64).empty());
}


TEST_P(TpmTest, authenticateCredential_mock)
{
    auto input = getTestHelper().getAuthenticateCredentialInput(*blobCache);
    const auto output = getTpm().authenticateCredential(std::move(input));

    ASSERT_FALSE(Base64::decode(output.plainTextCredentialBase64).empty());
}


TEST_P(TpmTest, authenticateCredential_production)
{
    auto input = getTestHelper().getAuthenticateCredentialInput(*blobCache);
    const auto output = getTpm().authenticateCredential(std::move(input));

    ASSERT_FALSE(Base64::decode(output.plainTextCredentialBase64).empty());
}


TEST_P(TpmTest, getQuote)
{
    Tpm::QuoteInput input;
    input.nonceBase64 = tpm::QuoteNONCESaved_bin_base64;
    input.pcrSet.push_back(0);
    input.hashAlgorithm = "SHA256";

    const auto output = getTpm().getQuote(std::move(input));

    ASSERT_FALSE(Base64::decode(output.quotedDataBase64).empty());
    ASSERT_FALSE(Base64::decode(output.quoteSignatureBase64).empty());
}


INSTANTIATE_TEST_SUITE_P(
    SimulatedTpm,
    TpmTest,
    testing::Values(createSimulatedTestDescriptorFactory()),
    [](auto&){return "simulated";});

INSTANTIATE_TEST_SUITE_P(
    MockedTpm,
    TpmTest,
    testing::Values(createMockTestDescriptorFactory()),
    [](auto&){return "mocked";});
