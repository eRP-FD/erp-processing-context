#ifndef ERP_PROCESSING_CONTEXT_HSMTESTBASE_HXX
#define ERP_PROCESSING_CONTEXT_HSMTESTBASE_HXX

#include "erp/hsm/ErpTypes.hxx"

#include <chrono>
#include <functional>

class BlobCache;
class HsmClient;
class HsmFactory;
class HsmSession;
class TeeTokenUpdater;


/**
 * Provide methods to set up tests that use the HSM so that either a mock or a remote simulator of the HSM is used,
 * depending on the configuration.
 */
class  HsmTestBase
{
public:
    const ErpVector attestationKeyName = ErpVector::create("<attestation key is 34 bytes long>");

    /**
     * (re) create client, blob cache and session.
     */
    void setupHsmTest (void);
    static bool isHsmSimulatorSupportedAndConfigured (void);

    static std::unique_ptr<HsmClient> createHsmClient (void);

    static std::unique_ptr<HsmFactory> createFactory (
        std::unique_ptr<HsmClient>&& client,
        std::shared_ptr<BlobCache> blobCache);

    static std::unique_ptr<TeeTokenUpdater> createTeeTokenUpdater (
        std::function<void(ErpBlob&&)>&& consumer,
        HsmFactory& hsmFactory,
        bool allowProductionUpdater,
        std::chrono::system_clock::duration updateInterval,
        std::chrono::system_clock::duration retryInterval);

    static ErpVector base64ToErpVector (const std::string_view base64data);

    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmFactory> mHsmFactory;
    std::unique_ptr<HsmSession> mHsmSession;
};


#endif
