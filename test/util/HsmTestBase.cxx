/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/HsmTestBase.hxx"

#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#if ! defined(__APPLE__)
    #include "erp/hsm/production/TeeTokenProductionUpdater.hxx"
    #include "erp/hsm/production/HsmProductionFactory.hxx"
    #include "erp/hsm/production/HsmProductionClient.hxx"
#endif

class TestTeeTokenUpdater : public TeeTokenUpdater
{
public:
    TestTeeTokenUpdater(HsmPool& hsmPool, TokenRefresher&& tokenRefresher,
                        std::chrono::system_clock::duration updateInterval,
                        std::chrono::system_clock::duration retryInterval)
        : TeeTokenUpdater(hsmPool, std::move(tokenRefresher), std::make_shared<Timer>(), updateInterval, retryInterval)
    {
    }
};


bool HsmTestBase::isHsmSimulatorSupportedAndConfigured (void)
{
    static const bool isSimulatorSupportedAndConfigured = mAllowProductionHsm &&
#if defined(__APPLE__) || defined(_WIN32)
        false;
#else
        ! Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE).empty();
#endif
    return isSimulatorSupportedAndConfigured;
}


void HsmTestBase::setupHsmTest(bool allowProductionUpdater, std::chrono::system_clock::duration updateInterval,
                               std::chrono::system_clock::duration retryInterval)
{
    mAllowProductionHsm = allowProductionUpdater;
    mBlobCache = MockBlobDatabase::createBlobCache(
        isHsmSimulatorSupportedAndConfigured()
            ? MockBlobCache::MockTarget::SimulatedHsm
            : MockBlobCache::MockTarget::MockedHsm
        );
    auto factory = createFactory(
        createHsmClient(),
        mBlobCache);
    mHsmPool = std::make_unique<HsmPool>(
        std::move(factory),
        [&, this](auto& hsmPool, std::shared_ptr<Timer>) {
            return createTeeTokenUpdater(hsmPool, updateInterval, retryInterval);
        },
        std::make_shared<Timer>());
}


std::unique_ptr<HsmClient> HsmTestBase::createHsmClient (void)
{
#if ! defined(__APPLE__) && ! defined(_WIN32)
    if (isHsmSimulatorSupportedAndConfigured())
        return std::make_unique<HsmProductionClient>();
    else
#endif
        return std::make_unique<HsmMockClient>();
}


std::unique_ptr<HsmFactory> HsmTestBase::createFactory (
    std::unique_ptr<HsmClient>&& client,
    std::shared_ptr<BlobCache> blobCache)
{
#if ! defined(__APPLE__) && ! defined(_WIN32)
    if (isHsmSimulatorSupportedAndConfigured())
        return std::make_unique<HsmProductionFactory>(std::move(client), std::move(blobCache));
    else
#endif
        return std::make_unique<HsmMockFactory>(std::move(client), std::move(blobCache));
}


std::unique_ptr<TeeTokenUpdater> HsmTestBase::createTeeTokenUpdater(HsmPool& hsmPool,
                                                                    std::chrono::system_clock::duration updateInterval,
                                                                    std::chrono::system_clock::duration retryInterval)
{
    TeeTokenUpdater::TokenRefresher tokenRefresher;
    if (isHsmSimulatorSupportedAndConfigured())
        tokenRefresher = [this](HsmPool& hsmPool) {
            TeeTokenProductionUpdater::refreshTeeToken(hsmPool);
            if (mUpdateCallback)
                mUpdateCallback();
        };
    else
        tokenRefresher = [this](HsmPool& hsmPool) {
            auto hsmPoolSession = hsmPool.acquire();
            auto& hsmSession = hsmPoolSession.session();
            hsmPool.setTeeToken(hsmSession.getTeeToken(ErpBlob(), ErpVector(), ErpVector()));
            if (mUpdateCallback)
                mUpdateCallback();
        };
    return std::make_unique<TestTeeTokenUpdater>(hsmPool, std::move(tokenRefresher), updateInterval, retryInterval);
}


ErpVector HsmTestBase::base64ToErpVector (const std::string_view base64data)
{
    return ErpVector::create(Base64::decodeToString(base64data));
}
