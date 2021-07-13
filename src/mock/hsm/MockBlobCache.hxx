#ifndef ERP_PROCESSING_CONTEXT_MOCKBLOBCACHE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKBLOBCACHE_HXX

#include <memory>

#if WITH_HSM_MOCK  != 1
#error MockBlobCache.hxx included but WITH_HSM_MOCK not enabled
#endif

class BlobCache;
class BlobDatabase;


/**
 * This class is not so much a mock of the blob cache itself but provides functions that help setup
 * a blob cache with mocked content.
 */
class MockBlobCache
{
public:
    /**
     * Depending on whether tests are directed at a mocked or a simulated HSM we need different values
     * for the blob contents.
     */
    enum class MockTarget
    {
        MockedHsm,
        SimulatedHsm
    };

    static std::shared_ptr<BlobCache> createAndSetup (
        MockTarget target,
        std::unique_ptr<BlobDatabase>&& blobDatabase);

    static std::shared_ptr<BlobCache> createBlobCache (
        MockTarget target);

    static void setupBlobCache (
        MockTarget target,
        BlobCache& blobCache);

private:
    static void setupBlobCacheForAllTargets (BlobCache& blobCache);
    static void setupBlobCacheForSimulatedHsm (BlobCache& blobCache);
    static void setupBlobCacheForMockedHsm (BlobCache& blobCache);
};

#endif
