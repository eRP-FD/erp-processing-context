/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

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
class HsmPool;


/**
 * Provide methods to set up tests that use the HSM so that either a mock or a remote simulator of the HSM is used,
 * depending on the configuration.
 */
class HsmTestBase
{
public:
    const ErpVector attestationKeyName = ErpVector::create("<attestation key is 34 bytes long>");

    /**
     * (re) create pool and blob cache
     */
    void setupHsmTest(bool allowProductionUpdater, std::chrono::system_clock::duration updateInterval,
                      std::chrono::system_clock::duration retryInterval);
    bool isHsmSimulatorSupportedAndConfigured();

    std::unique_ptr<TeeTokenUpdater> createTeeTokenUpdater(HsmPool& hsmPool,
                                                           std::chrono::system_clock::duration updateInterval,
                                                           std::chrono::system_clock::duration retryInterval);

    std::unique_ptr<HsmClient> createHsmClient();

    std::unique_ptr<HsmFactory> createFactory(std::unique_ptr<HsmClient>&& client,
                                                     std::shared_ptr<BlobCache> blobCache);

    static ErpVector base64ToErpVector(const std::string_view base64data);

    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::function<void()> mUpdateCallback;
    bool mAllowProductionHsm{true};
};


#endif
