/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef KEY_REFRESH_JOB_HXX
#define KEY_REFRESH_JOB_HXX

#include "erp/hsm/ErpTypes.hxx"
#include "erp/util/TimerJobBase.hxx"

class BlobCache;
class Configuration;
class HsmPool;

class PseudonameKeyRefreshJob : public TimerJobBase
{
public:
    PseudonameKeyRefreshJob(HsmPool& hsmPool, BlobCache& blobCache,
                            const std::chrono::steady_clock::duration checkInterval,
                            const std::chrono::steady_clock::duration failedCheckInterval,
                            const std::chrono::steady_clock::duration expireInterval);
    ~PseudonameKeyRefreshJob() noexcept override = default;

    static std::unique_ptr<PseudonameKeyRefreshJob> setupPseudonameKeyRefreshJob(HsmPool& hsmPool, BlobCache& blobCache,
                                                                                 const Configuration& configuration);

    /**
     * @brief Generate a hash from input with an existing key.
     * The key must exist in the blob cache.
     * @param input Source input to be hashed.
     * @returns A b64-encoded std::string hash. In case of failure, an empty string is returned.
     * Also, in case of a missing key, the returned value equals input.
     */
    static std::string hkdf(const std::string& input);

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

    /**
     * @brief Create a key for creating the pseudo name (hash).  The
     * key is retrieved from the HSM (or its mock) and stored in the
     * blob database, accessed with the blob cache. The expiration is
     * set to tNow + REPORT_KEY_REFRESH_INTERVAL.
     * @returns BlobId In case for further processing.
     */
    BlobId createAndStoreKey();

private:
    HsmPool& mHsmPool;
    BlobCache& mBlobCache;
    const std::chrono::steady_clock::duration mCheckInterval;
    const std::chrono::steady_clock::duration mFailedCheckInterval;
    const std::chrono::steady_clock::duration mExpireInterval;
};

#endif// #ifndef KEY_REFRESH_JOB_HXX
