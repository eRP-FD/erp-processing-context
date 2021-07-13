#include "test/util/HsmTestBase.hxx"

#include "erp/hsm/BlobCache.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/TLog.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"

#if ! defined(__APPLE__)
    #include "erp/hsm/production/TeeTokenProductionUpdater.hxx"
    #include "erp/hsm/production/HsmProductionFactory.hxx"
    #include "erp/hsm/production/HsmProductionClient.hxx"
#endif


bool HsmTestBase::isHsmSimulatorSupportedAndConfigured (void)
{
    static const bool isSimulatorSupportedAndConfigured =
#if defined(__APPLE__) || defined(_WIN32)
        false;
#else
        ! Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE).empty();
#endif
    return isSimulatorSupportedAndConfigured;
}


void HsmTestBase::setupHsmTest (void)
{
    mBlobCache = MockBlobCache::createBlobCache(
        isHsmSimulatorSupportedAndConfigured()
            ? MockBlobCache::MockTarget::SimulatedHsm
            : MockBlobCache::MockTarget::MockedHsm
        );
    mHsmFactory = createFactory(
        createHsmClient(),
        mBlobCache);
    mHsmSession = mHsmFactory->connect();
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

class TestTeeTokenUpdater : public TeeTokenUpdater
{
public:
    TestTeeTokenUpdater (
        TokenConsumer&& teeTokenConsumer,
        HsmFactory& hsmFactory,
        TokenProvider&& tokenProvider,
        std::chrono::system_clock::duration updateInterval,
        std::chrono::system_clock::duration retryInterval)
        : TeeTokenUpdater(std::move(teeTokenConsumer), hsmFactory, std::move(tokenProvider), updateInterval, retryInterval)
    {
    }
};

std::unique_ptr<TeeTokenUpdater> HsmTestBase::createTeeTokenUpdater (
    TeeTokenUpdater::TokenConsumer&& consumer,
    HsmFactory& hsmFactory,
    const bool allowProductionUpdater,
    std::chrono::system_clock::duration updateInterval,
    std::chrono::system_clock::duration retryInterval)
{
    TeeTokenUpdater::TokenProvider tokenProvider;
    if (allowProductionUpdater && isHsmSimulatorSupportedAndConfigured())
        tokenProvider = [](HsmFactory& factory){ return TeeTokenProductionUpdater::provideTeeToken(factory);};
    else
        tokenProvider = [](HsmFactory&){ return ErpBlob();};
    return std::make_unique<TestTeeTokenUpdater>(std::move(consumer), hsmFactory, std::move(tokenProvider), updateInterval, retryInterval);
}


ErpVector HsmTestBase::base64ToErpVector (const std::string_view base64data)
{
    return ErpVector::create(Base64::decodeToString(base64data));
}
