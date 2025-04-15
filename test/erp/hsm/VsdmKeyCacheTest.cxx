/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h> // MUST BE FIRST (for friend tests)

#include "shared/hsm/VsdmKeyCache.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Random.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"

#include <memory>

class CountingHsmMockClient : public HsmMockClient
{
public:
    ErpVector unwrapRawPayload(const HsmRawSession& session, UnwrapRawPayloadInput&& input) override
    {
        if (unwrapPayloadCallback)
            unwrapPayloadCallback();
        return HsmMockClient::unwrapRawPayload(session, std::move(input));
    }

    void setUnwrapPayloadCallback(const std::function<void()>& callback)
    {
        unwrapPayloadCallback = callback;
    }

private:
    std::function<void()> unwrapPayloadCallback;
};


class VsdmKeyCacheTest : public testing::Test
{
public:
    void SetUp() override
    {
        unwrapPayloadCounter = 0;

        auto keyDb = std::make_unique<MockVsdmKeyBlobDatabase>();
        keyDb->storeBlob(vsdmKeyBlobFromPackage(defaultVsdmKeyPackage));
        vsdmKeyCache = std::make_unique<VsdmKeyCache>(std::move(keyDb), hsmPool);
        ASSERT_NO_FATAL_FAILURE(
            vsdmKeyCache->getKey(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version()));
    }


    VsdmKeyBlobDatabase::Entry vsdmKeyBlobFromPackage(const VsdmHmacKey& vsdmKeyPackage)
    {
        const ErpVector vsdmKeyData = ErpVector::create(vsdmKeyPackage.serializeToString());

        auto hsmPoolSession = hsmPool.acquire();
        auto& hsmSession = hsmPoolSession.session();
        VsdmKeyBlobDatabase::Entry blobEntry;
        blobEntry.operatorId = vsdmKeyPackage.operatorId();
        blobEntry.version = vsdmKeyPackage.version();
        blobEntry.createdDateTime = std::chrono::system_clock::now();
        blobEntry.blob = hsmSession.wrapRawPayload(vsdmKeyData, 0);
        return blobEntry;
    }

    VsdmHmacKey generateVsdmPackage(char operatorId, char version,
                                    const util::Buffer& key = Random::randomBinaryData(VsdmHmacKey::keyLength))
    {
        VsdmHmacKey package{operatorId, version};
        package.setPlainTextKey(Base64::encode(key));
        return package;
    }

    std::unique_ptr<CountingHsmMockClient> createCountingMockClient()
    {
        auto hsmMock = std::make_unique<CountingHsmMockClient>();
        hsmMock->setUnwrapPayloadCallback([this]() {
            unwrapPayloadCounter++;
        });
        return hsmMock;
    }

    HsmPool hsmPool{
        std::make_unique<HsmMockFactory>(createCountingMockClient(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
    std::size_t unwrapPayloadCounter{};
    std::unique_ptr<VsdmKeyCache> vsdmKeyCache;
    VsdmHmacKey defaultVsdmKeyPackage = generateVsdmPackage('V', '1');
};


TEST_F(VsdmKeyCacheTest, getKey_success)
{
    // ensure the initial key is already present in the cache
    auto key = vsdmKeyCache->getKey(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version());
    // the package contains the key base64 encoded, the cache returns it in binary
    ASSERT_EQ(defaultVsdmKeyPackage.plainTextKey(), Base64::encode(static_cast<std::string_view>(key)));
    ASSERT_EQ(unwrapPayloadCounter, 1);
    // ensure the cache does try to unwrap the key a second time
    vsdmKeyCache->getKey(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version());
    ASSERT_EQ(unwrapPayloadCounter, 1);

    ASSERT_THROW(vsdmKeyCache->getKey('V', '3'), ErpException);
    ASSERT_EQ(unwrapPayloadCounter, 1);
}

TEST_F(VsdmKeyCacheTest, keyUpdate)
{
    ASSERT_EQ(unwrapPayloadCounter, 1);
    // insert a new key into the database and generate a cache-miss
    const auto packageV2 = generateVsdmPackage('V', '2');
    auto& keyDb = vsdmKeyCache->getVsdmKeyBlobDatabase();
    keyDb.storeBlob(vsdmKeyBlobFromPackage(packageV2));
    auto key = vsdmKeyCache->getKey(packageV2.operatorId(), packageV2.version());
    ASSERT_EQ(packageV2.plainTextKey(), Base64::encode(static_cast<std::string_view>(key)));
    ASSERT_EQ(unwrapPayloadCounter, 2);
}

TEST_F(VsdmKeyCacheTest, keyRemoval)
{
    ASSERT_EQ(unwrapPayloadCounter, 1);
    auto& keyDb = vsdmKeyCache->getVsdmKeyBlobDatabase();
    keyDb.deleteBlob(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version());
    // the cache wont notice that the key has been deleted
    ASSERT_NO_FATAL_FAILURE(vsdmKeyCache->getKey(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version()));
    vsdmKeyCache->rebuildCache();
    ASSERT_EQ(unwrapPayloadCounter, 1);

    ASSERT_THROW(vsdmKeyCache->getKey(defaultVsdmKeyPackage.operatorId(), defaultVsdmKeyPackage.version()),
                 ErpException);
}


TEST_F(VsdmKeyCacheTest, skipFailingUnwrap)
{
    // this test ensures that the vsdm key cache can still successfully
    // created, even if unwrapPayload fails for a single key
    auto hsmclient = std::make_unique<CountingHsmMockClient>();
    int unwrapCalls = 0;
    hsmclient->setUnwrapPayloadCallback([&unwrapCalls]() {
        // only fail on the first call, so we can add new data that is not
        // failing afterwards
        unwrapCalls++;
        if (unwrapCalls == 1)
        {
            // do not throw an HsmException, otherwise HsmSession will retry the call
            throw std::runtime_error("Simulated HSM error");
        }
    });
    HsmPool hsmPool{
        std::make_unique<HsmMockFactory>(std::move(hsmclient),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};

    auto keyDb = std::make_unique<MockVsdmKeyBlobDatabase>();
    // add two which are okay themselves
    keyDb->storeBlob(vsdmKeyBlobFromPackage(defaultVsdmKeyPackage));
    keyDb->storeBlob(vsdmKeyBlobFromPackage(generateVsdmPackage('V', '2')));
    // add a broken vsdm package
    auto packageV3 = generateVsdmPackage('V', '3');
    packageV3.deleteKeyInformation();
    keyDb->storeBlob(vsdmKeyBlobFromPackage(packageV3));
    ASSERT_EQ(unwrapCalls, 0);
    // the creation of the key cache will fail with the first key (unwrap fails) and
    // and the last one with a broken vsdm package
    ASSERT_NO_THROW(vsdmKeyCache = std::make_unique<VsdmKeyCache>(std::move(keyDb), hsmPool));
    ASSERT_EQ(unwrapCalls, 3);
    ASSERT_EQ(vsdmKeyCache->mEntryCache.size(), 1);
}
